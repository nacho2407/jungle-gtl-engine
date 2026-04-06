#include "Renderer/SceneProxy.h"

#include "Component/LineBatchComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/ObjectUniformStream.h"
#include "Renderer/Renderer.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/ViewInfo.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent)
	: Bounds(InComponent ? InComponent->GetWorldBounds() : FBoxSphereBounds{})
{
}

void FPrimitiveSceneProxy::CollectMeshBatchesForRenderMesh(const FViewInfo& View, FRenderMesh* InRenderMesh, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	(void)InRenderMesh;
	CollectMeshBatches(View, Renderer, OutMeshBatches);
}

bool FPrimitiveSceneProxy::TryBuildStaticMeshOcclusionCandidate(const FVector& CameraPosition, FRenderMesh*& OutRenderMesh, FStaticMeshOcclusionCandidate& OutCandidate) const
{
	(void)CameraPosition;
	OutRenderMesh = nullptr;
	OutCandidate = {};
	return false;
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	if (!InComponent)
	{
		return;
	}

	Component = InComponent;
	ComponentId = InComponent->UUID;
	StaticMesh = InComponent->GetStaticMesh();
	LocalToWorld = InComponent->GetWorldTransform();
	if (!StaticMesh)
	{
		return;
	}

	CachedOcclusionCandidate.MatchKey = MakeStaticMeshOcclusionMatchKey(ComponentId);
	CachedOcclusionCandidate.BoundsCenter = Bounds.Center;
	CachedOcclusionCandidate.BoundsRadius = Bounds.Radius;
	CachedOcclusionCandidate.BoundsExtent = Bounds.BoxExtent;

	FRenderMesh* BaseRenderMesh = InComponent->GetRenderMesh();
	const int32 SectionCount = BaseRenderMesh ? BaseRenderMesh->GetNumSection() : 0;
	CachedDrawCallCount = SectionCount > 0 ? static_cast<uint32>(SectionCount) : 1u;
	const int32 MaterialCount = (std::max)(InComponent->GetNumMaterials(), SectionCount);
	Materials.reserve(static_cast<size_t>((std::max)(MaterialCount, 1)));
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		const std::shared_ptr<FMaterial> Material = InComponent->GetMaterial(MaterialIndex);
		Materials.push_back(Material ? Material.get() : nullptr);
	}
}

void FStaticMeshSceneProxy::CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	CollectMeshBatchesForRenderMesh(View, ResolveRenderMesh(View.CameraPosition), Renderer, OutMeshBatches);
}

void FStaticMeshSceneProxy::CollectMeshBatchesForRenderMesh(const FViewInfo& View, FRenderMesh* InRenderMesh, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const
{
	(void)View;
	if (!InRenderMesh)
	{
		return;
	}

	const int32 SectionCount = InRenderMesh->GetNumSection();
	if (SectionCount <= 0)
	{
		FMeshRenderItem MeshBatch = {};
		MeshBatch.Material = !Materials.empty() && Materials[0] ? Materials[0] : Renderer.GetDefaultMaterial();
		MeshBatch.RenderMesh = InRenderMesh;
		MeshBatch.WorldMatrix = LocalToWorld;
		MeshBatch.RenderPass = ERenderPass::Opaque;
		MeshBatch.bStaticMesh = true;
		OutMeshBatches.push_back(MeshBatch);
		return;
	}

	OutMeshBatches.reserve(OutMeshBatches.size() + SectionCount);
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FMeshSection& Section = InRenderMesh->Sections[SectionIndex];

		FMeshRenderItem MeshBatch = {};
		MeshBatch.Material = SectionIndex < static_cast<int32>(Materials.size()) ? Materials[SectionIndex] : nullptr;
		if (!MeshBatch.Material)
		{
			MeshBatch.Material = Renderer.GetDefaultMaterial();
		}
		MeshBatch.RenderMesh = InRenderMesh;
		MeshBatch.WorldMatrix = LocalToWorld;
		MeshBatch.IndexStart = Section.StartIndex;
		MeshBatch.IndexCount = Section.IndexCount;
		MeshBatch.SectionIndex = static_cast<uint32>(SectionIndex);
		MeshBatch.RenderPass = ERenderPass::Opaque;
		MeshBatch.bStaticMesh = true;
		OutMeshBatches.push_back(std::move(MeshBatch));
	}
}

bool FStaticMeshSceneProxy::TryBuildStaticMeshOcclusionCandidate(const FVector& CameraPosition, FRenderMesh*& OutRenderMesh, FStaticMeshOcclusionCandidate& OutCandidate) const
{
	const FRenderMesh* SelectedMesh = ResolveRenderMesh(CameraPosition);
	if (!SelectedMesh)
	{
		OutRenderMesh = nullptr;
		OutCandidate = {};
		return false;
	}

	OutRenderMesh = const_cast<FRenderMesh*>(SelectedMesh);
	BuildStaticMeshOcclusionCandidate(OutCandidate);
	return true;
}

void FStaticMeshSceneProxy::BuildStaticMeshOcclusionCandidate(FStaticMeshOcclusionCandidate& OutCandidate) const
{
	OutCandidate = CachedOcclusionCandidate;
}

FRenderMesh* FStaticMeshSceneProxy::ResolveRenderMesh(const FVector& CameraPosition) const
{
	if (!StaticMesh)
	{
		return nullptr;
	}

	const float DistanceSquared = (Bounds.Center - CameraPosition).SizeSquared();
	return StaticMesh->GetRenderDataForDistanceSquared(DistanceSquared);
}

FMaterial* FStaticMeshSceneProxy::GetMaterialForSection(int32 SectionIndex) const
{
	return SectionIndex >= 0 && SectionIndex < static_cast<int32>(Materials.size()) ? Materials[SectionIndex] : nullptr;
}

uint32 FStaticMeshSceneProxy::ResolveObjectUniformAllocation(FObjectUniformStream& ObjectUniformStream) const
{
	if (CachedObjectUniformAllocation == UINT32_MAX)
	{
		CachedObjectUniformAllocation = ObjectUniformStream.AcquireStaticWorldMatrix(ComponentId, LocalToWorld);
	}

	return CachedObjectUniformAllocation;
}

uint32 FStaticMeshSceneProxy::EstimateDrawCallCount() const
{
	return CachedDrawCallCount;
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
	RenderPass = InComponent->IsA(UUUIDBillboardComponent::StaticClass()) ? ERenderPass::NoDepth : ERenderPass::Alpha;

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
	MeshBatch.RenderPass = ERenderPass::Alpha;
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
