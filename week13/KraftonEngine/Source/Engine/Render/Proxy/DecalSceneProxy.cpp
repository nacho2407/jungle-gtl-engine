#include "Render/Proxy/DecalSceneProxy.h"

#include "Component/DecalComponent.h"
#include "Render/Resource/ShaderManager.h"

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
	// 최초 1회 초기화
	UpdateMesh();
}

FDecalSceneProxy::~FDecalSceneProxy()
{
	DecalCB.Release();
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

	DecalTexture = DecalComp->GetTexture();
	DiffuseSRV = DecalTexture ? DecalTexture->SRV : nullptr;

	auto& CB = ExtraCB.Bind<FDecalConstants>(&DecalCB, ECBSlot::PerShader0);
	CB.WorldToDecal = DecalComp->GetWorldMatrix().GetInverse();
	CB.Color = DecalComp->GetColor();
}

void FDecalSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	MeshBuffer = nullptr;
	SectionDraws.clear();
	Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	Pass = ERenderPass::Decal;
	bSupportsOutline = false;
}
