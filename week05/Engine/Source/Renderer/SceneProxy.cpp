#include "Renderer/SceneProxy.h"

#include "Component/LineBatchComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/Renderer.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/ViewInfo.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent)
	: Bounds(InComponent ? InComponent->GetWorldBounds() : FBoxSphereBounds{})
{
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	if (!InComponent)
	{
		return;
	}

	RenderMesh = InComponent->GetRenderMesh();
	LocalToWorld = InComponent->GetWorldTransform();
	if (!RenderMesh)
	{
		return;
	}

	const int32 MaterialCount = (std::max)(InComponent->GetNumMaterials(), RenderMesh->GetNumSection());
	Materials.reserve(static_cast<size_t>((std::max)(MaterialCount, 1)));
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const std::shared_ptr<FMaterial> Material = InComponent->GetMaterial(MaterialIndex);
		Materials.push_back(Material ? Material.get() : nullptr);
	}

	const int32 SectionCount = RenderMesh->GetNumSection();
	if (SectionCount <= 0)
	{
		FMeshRenderItem MeshBatch = {};
		MeshBatch.Material = !Materials.empty() ? Materials[0] : nullptr;
		MeshBatch.RenderMesh = RenderMesh;
		MeshBatch.WorldMatrix = LocalToWorld;
		MeshBatchTemplates.push_back(MeshBatch);
		return;
	}

	MeshBatchTemplates.reserve(SectionCount);
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FMeshSection& Section = RenderMesh->Sections[SectionIndex];

		FMeshRenderItem MeshBatch = {};
		MeshBatch.Material = SectionIndex < static_cast<int32>(Materials.size()) ? Materials[SectionIndex] : nullptr;
		MeshBatch.RenderMesh = RenderMesh;
		MeshBatch.WorldMatrix = LocalToWorld;
		MeshBatch.IndexStart = Section.StartIndex;
		MeshBatch.IndexCount = Section.IndexCount;
		MeshBatch.SectionIndex = static_cast<uint32>(SectionIndex);
		MeshBatchTemplates.push_back(MeshBatch);
	}
}

void FStaticMeshSceneProxy::CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	(void)View;

	if (!RenderMesh)
	{
		return;
	}

	OutMeshBatches.reserve(OutMeshBatches.size() + MeshBatchTemplates.size());
	for (const FMeshRenderItem& TemplateBatch : MeshBatchTemplates)
	{
		FMeshRenderItem MeshBatch = TemplateBatch;
		if (!MeshBatch.Material)
		{
			MeshBatch.Material = Renderer.GetDefaultMaterial();
		}
		OutMeshBatches.push_back(MeshBatch);
	}
}

FTextSceneProxy::FTextSceneProxy(const UTextComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	if (!InComponent)
	{
		return;
	}

	LocalToWorld = InComponent->GetWorldTransform();
	RenderWorldPosition = InComponent->GetRenderWorldPosition();
	RenderWorldScale = InComponent->GetRenderWorldScale();
	TextColor = InComponent->GetTextColor();
	DisplayText = InComponent->GetDisplayText();
	TextScale = InComponent->GetTextScale();
	bBillboard = InComponent->IsBillboard();
	RenderPass = InComponent->IsA(UUUIDBillboardComponent::StaticClass()) ? ERenderPass::NoDepth : ERenderPass::World;

	TextMesh = std::make_shared<FDynamicMesh>();
	TextMesh->Topology = EMeshTopology::EMT_TriangleList;
}

FTextSceneProxy::~FTextSceneProxy() = default;

void FTextSceneProxy::CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	if (DisplayText.empty() || !TextMesh)
	{
		return;
	}

	if (bMeshDirty)
	{
		TextMesh->Vertices.clear();
		TextMesh->Indices.clear();
		bMeshDirty = !Renderer.GetTextRenderer().BuildTextMesh(DisplayText, *TextMesh);
		TextMesh->bIsDirty = true;
	}

	if (TextMesh->Vertices.empty())
	{
		return;
	}

	if (!DynamicMaterial)
	{
		if (FMaterial* FontMaterial = Renderer.GetTextRenderer().GetFontMaterial())
		{
			DynamicMaterial = FontMaterial->CreateDynamicMaterial();
		}
	}

	FMaterial* Material = DynamicMaterial ? static_cast<FMaterial*>(DynamicMaterial.get()) : Renderer.GetTextRenderer().GetFontMaterial();
	if (!Material)
	{
		return;
	}

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameter("TextColor", TextColor);
	}

	FMeshRenderItem MeshBatch = {};
	MeshBatch.Material = Material;
	MeshBatch.RenderMesh = TextMesh.get();
	MeshBatch.WorldMatrix = bBillboard
		? FMatrix::MakeScale(RenderWorldScale) * FMatrix::MakeBillboard(RenderWorldPosition, View.CameraPosition)
		: FMatrix::MakeScale(FVector(TextScale, TextScale, TextScale)) * LocalToWorld;
	MeshBatch.RenderPass = RenderPass;
	OutMeshBatches.push_back(MeshBatch);
}

FSubUVSceneProxy::FSubUVSceneProxy(const USubUVComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	if (!InComponent)
	{
		return;
	}

	LocalToWorld = InComponent->GetWorldTransform();
	Size = InComponent->GetSize();
	Columns = InComponent->GetColumns();
	Rows = InComponent->GetRows();
	TotalFrames = InComponent->GetTotalFrames();
	FirstFrame = InComponent->GetFirstFrame();
	LastFrame = InComponent->GetLastFrame();
	FPS = InComponent->GetFPS();
	bLoop = InComponent->IsLoop();
	bBillboard = InComponent->IsBillboard();

	SubUVMesh = std::make_shared<FDynamicMesh>();
	SubUVMesh->Topology = EMeshTopology::EMT_TriangleList;
}

FSubUVSceneProxy::~FSubUVSceneProxy() = default;

void FSubUVSceneProxy::CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	if (!SubUVMesh)
	{
		return;
	}

	if (bMeshDirty)
	{
		SubUVMesh->Vertices.clear();
		SubUVMesh->Indices.clear();
		bMeshDirty = !Renderer.GetSubUVRenderer().BuildSubUVMesh(Size, *SubUVMesh);
		SubUVMesh->bIsDirty = true;
	}

	if (SubUVMesh->Vertices.empty())
	{
		return;
	}

	if (!DynamicMaterial)
	{
		if (FMaterial* BaseMaterial = Renderer.GetSubUVRenderer().GetSubUVMaterial())
		{
			DynamicMaterial = BaseMaterial->CreateDynamicMaterial();
		}
	}

	FMaterial* Material = DynamicMaterial ? static_cast<FMaterial*>(DynamicMaterial.get()) : Renderer.GetSubUVRenderer().GetSubUVMaterial();
	if (!Material)
	{
		return;
	}

	Renderer.GetSubUVRenderer().UpdateAnimationParams(
		Material,
		Columns,
		Rows,
		TotalFrames,
		FirstFrame,
		LastFrame,
		FPS,
		View.Time,
		bLoop);

	FMatrix WorldMatrix = LocalToWorld;
	if (bBillboard)
	{
		const FVector WorldPosition = WorldMatrix.GetTranslation();
		const FVector WorldScale = WorldMatrix.GetScaleVector();
		WorldMatrix = FMatrix::MakeScale(WorldScale) * FMatrix::MakeBillboard(WorldPosition, View.CameraPosition);
	}

	FMeshRenderItem MeshBatch = {};
	MeshBatch.Material = Material;
	MeshBatch.RenderMesh = SubUVMesh.get();
	MeshBatch.WorldMatrix = WorldMatrix;
	OutMeshBatches.push_back(MeshBatch);
}

FLineBatchSceneProxy::FLineBatchSceneProxy(const ULineBatchComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	if (!InComponent)
	{
		return;
	}

	RenderMesh = InComponent->GetRenderMesh();
	LocalToWorld = InComponent->GetWorldTransform();
}

void FLineBatchSceneProxy::CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	(void)View;

	if (!RenderMesh)
	{
		return;
	}

	FMeshRenderItem MeshBatch = {};
	MeshBatch.Material = Renderer.GetDefaultMaterial();
	MeshBatch.RenderMesh = RenderMesh;
	MeshBatch.WorldMatrix = LocalToWorld;
	OutMeshBatches.push_back(MeshBatch);
}
