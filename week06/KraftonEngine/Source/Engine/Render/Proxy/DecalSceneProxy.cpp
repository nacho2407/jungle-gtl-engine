#include "DecalSceneProxy.h"
#include "Component/DecalComponent.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Pipeline/RenderBus.h"

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
}

void FDecalSceneProxy::UpdateTransform()
{
	UDecalComponent* Comp = GetDecalComponent();
	if (!Comp) return;

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Comp->GetDecalToWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = Comp->GetWorldBoundingBox();
	LastLODUpdateFrame = UINT32_MAX;
	MarkPerObjectCBDirty();
}

void FDecalSceneProxy::UpdateMesh()
{
	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Cube);
	Shader = FShaderManager::Get().GetShader(EShaderType::Decal);
	Pass = ERenderPass::Decal;
	RebuildSectionDraw();
}

void FDecalSceneProxy::UpdateMaterial()
{
	RebuildSectionDraw();
}

void FDecalSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	const UDecalComponent* Comp = GetDecalComponent();
	if (!Comp) return;

	auto& DecalCB = ExtraCB.Bind<FDecalConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Decal, sizeof(FDecalConstants)),
		ECBSlot::Decal);

	DecalCB.InvView = Bus.GetView().GetInverseFast();
	DecalCB.InvProjection = Bus.GetProj().GetInverse();
	DecalCB.WorldToDecal = Comp->GetWorldToDecalMatrix();
	DecalCB.DecalForward = FVector(1.0f, 0.0f, 0.0f);
	DecalCB.DecalOpacity = Comp->GetFadeAlpha();
}

void FDecalSceneProxy::RebuildSectionDraw()
{
	SectionDraws.clear();

	if (!MeshBuffer)
	{
		UpdateSortKey();
		return;
	}

	FMeshSectionDraw Draw = {};
	Draw.FirstIndex = 0;
	Draw.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

	const UDecalComponent* Comp = GetDecalComponent();
	const FTextureResource* Texture = Comp ? Comp->GetTexture() : nullptr;
	if (Texture)
	{
		Draw.DiffuseSRV = Texture->SRV;
	}

	SectionDraws.push_back(Draw);
	UpdateSortKey();
}

UDecalComponent* FDecalSceneProxy::GetDecalComponent() const
{
	return static_cast<UDecalComponent*>(Owner);
}
