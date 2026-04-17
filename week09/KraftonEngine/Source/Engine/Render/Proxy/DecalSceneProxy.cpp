#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
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
	return static_cast<UDecalComponent*>(Owner);
}

void FDecalSceneProxy::UpdateMaterial()
{
	UDecalComponent* DecalComp = GetDecalComponent();
	if (!DecalComp)
	{
		return;
	}

	DecalMaterial = DecalComp->GetMaterial(0);
	DiffuseSRV = nullptr;

	if (DecalMaterial)
	{
		UTexture2D* DiffuseTex = nullptr;
		if (DecalMaterial->GetTextureParameter("DiffuseTexture", DiffuseTex))
		{
			DiffuseSRV = DiffuseTex->GetSRV();
		}
	}

	auto& CB = ExtraCB.Bind<FDecalConstants>(DecalCB, ECBSlot::PerShader0);
	CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
	CB.Color = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	MeshBuffer = nullptr;
	SectionDraws.clear();

	if (DecalMaterial && DecalMaterial->GetShader())
	{
		Shader = DecalMaterial->GetShader();
		Pass = DecalMaterial->GetRenderPass();

		// 머티리얼 기반 렌더 상태 전파
		Blend = DecalMaterial->GetBlendState();
		DepthStencil = DecalMaterial->GetDepthStencilState();
		Rasterizer = DecalMaterial->GetRasterizerState();
	}
	else
	{
		Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
		Pass = ERenderPass::Decal;

		// 기본 상태
		Blend = EBlendState::AlphaBlend;
		DepthStencil = EDepthStencilState::DepthReadOnly;
		Rasterizer = ERasterizerState::SolidNoCull;
	}
	bSupportsOutline = false;
}
