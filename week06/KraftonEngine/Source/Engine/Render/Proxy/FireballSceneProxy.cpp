#include "FireballSceneProxy.h"
#include "Component/FireballComponent.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Pipeline/RenderBus.h"


FFireballSceneProxy::FFireballSceneProxy(UFireballComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
}

void FFireballSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	const UFireballComponent* Comp = GetFireballComponent();
	if (!Comp) return;

	auto& FireballCB = ExtraCB.Bind<FFireballConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Fireball, sizeof(FFireballConstants)),
		ECBSlot::Fireball);

	FireballCB.Color = Comp->GetColor();
	FireballCB.Radius = Comp->GetRadius();
	FireballCB.RadiusFalloff = Comp->GetRadiusFalloff();
}

void FFireballSceneProxy::UpdateMesh()
{
	MeshBuffer = nullptr;
	Shader = FShaderManager::Get().GetShader(EShaderType::Fireball);
}

UFireballComponent* FFireballSceneProxy::GetFireballComponent() const
{
	return static_cast<UFireballComponent*>(Owner);
}
