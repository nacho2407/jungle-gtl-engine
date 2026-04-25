#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Shader/ShaderManager.h"

#include "Materials/Material.h"
#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"

namespace
{
	struct FDecalConstants
	{
		FMatrix WorldToDecal;
		FVector4 Color;
	};
}

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::Decal;
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
	DecalCB = new FConstantBuffer();
	// 최초 1회 초기화
	UpdateMesh();
}

FDecalSceneProxy::~FDecalSceneProxy()
{
	if (DecalCB)
	{
		DecalCB->Release();
		delete DecalCB;
		DecalCB = nullptr;
	}
	if (DecalProxyMaterial)
	{
		UObjectManager::Get().DestroyObject(DecalProxyMaterial);
		DecalProxyMaterial = nullptr;
	}
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(GetOwner());
}

void FDecalSceneProxy::UpdateMaterial()
{
	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp) return;

	DecalMaterial = DecalComp->GetMaterial();

	// 프록시 전용 transient Material 래퍼 생성 (공유 DecalMaterial에 직접 CB를 쓸 수 없음)
	if (!DecalProxyMaterial)
	{
		FShader* Shader = (DecalMaterial && DecalMaterial->GetShader())
			? DecalMaterial->GetShader()
			: FShaderManager::Get().GetOrCreate(EShaderPath::Decal);
		ERenderPass Pass = DecalMaterial ? DecalMaterial->GetRenderPass() : ERenderPass::Decal;
		EBlendState Blend = DecalMaterial ? DecalMaterial->GetBlendState() : EBlendState::Opaque;
		EDepthStencilState Depth = DecalMaterial ? DecalMaterial->GetDepthStencilState() : EDepthStencilState::Default;
		ERasterizerState Raster = DecalMaterial ? DecalMaterial->GetRasterizerState() : ERasterizerState::SolidBackCull;

		DecalProxyMaterial = UMaterial::CreateTransient(Pass, Blend, Depth, Raster, Shader);
	}

	// SRV 동기화 (DecalMaterial의 텍스처를 래퍼에 복사)
	if (DecalMaterial)
	{
		const ID3D11ShaderResourceView* const* SRVs = DecalMaterial->GetCachedSRVs();
		for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
			DecalProxyMaterial->SetCachedSRV(static_cast<EMaterialTextureSlot>(s),
				const_cast<ID3D11ShaderResourceView*>(SRVs[s]));
	}

	// Per-shader CB (WorldToDecal, Color) 바인딩
	auto& CB = DecalProxyMaterial->BindPerShaderCB<FDecalConstants>(DecalCB, ECBSlot::PerShader0);
	CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
	CB.Color = DecalComp->GetColor();

	// SectionDraws — 래퍼 Material 사용
	SectionDraws.clear();
	SectionDraws.push_back({ DecalProxyMaterial, 0, 0 });
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();
	RebuildReceiverProxies();

	MeshBuffer = nullptr;
	ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
}

void FDecalSceneProxy::RebuildReceiverProxies()
{
	CachedReceiverProxies.clear();

	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp) return;

	for (UStaticMeshComponent* Receiver : DecalComp->GetReceivers())
	{
		if (Receiver)
		{
			FPrimitiveSceneProxy* ReceiverProxy = Receiver->GetSceneProxy();
			if (ReceiverProxy)
				CachedReceiverProxies.push_back(ReceiverProxy);
		}
	}
}
