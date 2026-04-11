#include "FireballSceneProxy.h"
#include "Component/FireballComponent.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Resource/MeshBufferManager.h"


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

	Pass = ERenderPass::Decal;

	FireballCB.FireballInvView = Bus.GetView().GetInverseFast();
	FireballCB.FireballInvProj = Bus.GetProj().GetInverse();
	FireballCB.Color = Comp->GetColor().ToVector4();
	FireballCB.Intensity = Comp->GetIntensity();
	FireballCB.Radius = Comp->GetRadius();
	FireballCB.RadiusFalloff = Comp->GetRadiusFalloff();
}

void FFireballSceneProxy::UpdateMesh()
{
	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Sphere);
	Shader = FShaderManager::Get().GetShader(EShaderType::Fireball);
}

UFireballComponent* FFireballSceneProxy::GetFireballComponent() const
{
	return static_cast<UFireballComponent*>(Owner);
}
