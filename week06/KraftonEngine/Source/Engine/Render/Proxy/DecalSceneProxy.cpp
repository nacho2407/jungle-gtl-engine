#include "DecalSceneProxy.h"
#include "Component/DecalComponent.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"

FDecalSceneProxy::FDecalSceneProxy(UDecalComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
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
	Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Pass = ERenderPass::Decal;
	RebuildSectionDraw();
}

void FDecalSceneProxy::UpdateMaterial()
{
	RebuildSectionDraw();
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
