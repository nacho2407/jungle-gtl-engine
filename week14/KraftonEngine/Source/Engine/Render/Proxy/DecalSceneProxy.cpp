#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Resource/ShaderManager.h"

#include "Materials/Material.h"
#include "Texture/Texture2D.h"

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
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(GetOwner());
}

void FDecalSceneProxy::UpdateMaterial()
{
	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp)
	{
		return;
	}

	DecalMaterial = DecalComp->GetMaterial();

	// SectionDraws 단일 항목으로 Material 관리 (IndexCount=0: Decal은 자체 draw 안 함)
	SectionDraws.clear();
	if (DecalMaterial)
	{
		SectionDraws.push_back({ DecalMaterial, nullptr, 0, 0 });
	}

	auto& CB = ExtraCB.Bind<FDecalConstants>(DecalCB, ECBSlot::PerShader0);
	CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
	CB.Color = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();
	RebuildReceiverProxies();

	MeshBuffer = nullptr;
	SectionDraws.clear();

	if (DecalMaterial && DecalMaterial->GetShader())
	{
		Shader = DecalMaterial->GetShader();
		Pass = DecalMaterial->GetRenderPass();
	}
	else
	{
		Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
		Pass = ERenderPass::Decal;
	}
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
