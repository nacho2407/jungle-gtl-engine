#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/DecalComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/FireballComponent.h"
#include "Core/ResourceManager.h"
#include "Engine/Geometry/Frustum.h"
#include "Engine/Asset/StaticMesh.h"
#include "Render/Resource/Material.h"
#include "Object/ObjectIterator.h"
#include "Runtime/Stats/ScopeCycleCounter.h"
#include <unordered_set>

namespace
{
	FColor MakeBVHInternalNodeColor(int32 PathIndexFromLeaf, int32 PathLength)
	{
		if (PathLength <= 1)
		{
			return FColor::Yellow();
		}

		const float T = static_cast<float>(PathIndexFromLeaf) / static_cast<float>(PathLength - 1);
		return FColor::Lerp(FColor::Cyan(), FColor::Yellow(), T);
	}

	bool UsesCameraDependentRenderBounds(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (PrimitiveComponent == nullptr)
		{
			return false;
		}

		switch (PrimitiveComponent->GetPrimitiveType())
		{
		case EPrimitiveType::EPT_Billboard:
		case EPrimitiveType::EPT_Text:
		case EPrimitiveType::EPT_SubUV:
			return true;
		default:
			return false;
		}
	}

	FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
	{
		const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			WorldMatrix.GetOrigin(),
			WorldMatrix.GetScaleVector(),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}

	FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
	{
		const FVector WorldScale = SubUVComp->GetWorldScale();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			SubUVComp->GetWorldLocation(),
			FVector(
				WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
				SubUVComp->GetWidth() * WorldScale.Y,
				SubUVComp->GetHeight() * WorldScale.Z),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}
	/*
	* BillBoardComponent를 상속받은 text, SubUV가 사용하는 AABB 계산함수(의존성 분리)
	*/
	FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
	{
		static constexpr FVector LocalQuadCorners[4] =
		{
			FVector(0.0f, -0.5f,  0.5f),
			FVector(0.0f,  0.5f,  0.5f),
			FVector(0.0f,  0.5f, -0.5f),
			FVector(0.0f, -0.5f, -0.5f)
		};

		FAABB Box;
		Box.Reset();

		for (const FVector& Corner : LocalQuadCorners)
		{
			Box.Expand(WorldMatrix.TransformPosition(Corner));
		}

		return Box;
	}

	FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
	{
		switch (PrimitiveComponent->GetPrimitiveType())
		{
		case EPrimitiveType::EPT_Billboard:
			return BuildQuadAABB(MakeViewBillboardMatrix(PrimitiveComponent, RenderBus));
		case EPrimitiveType::EPT_Text:
		{
			const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
			return BuildQuadAABB(TextComp->GetTextMatrix());
		}
		case EPrimitiveType::EPT_SubUV:
		{
			const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
			return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
		}

		default:
			return PrimitiveComponent->GetWorldAABB();
		}
	}

	int32 SelectLODLevel(const FVector& CameraPos, const FAABB& Bounds, const FMatrix& ProjMatrix, int32 ValidLODCount)
	{
		bool IsOrthoGraphic = (std::abs(ProjMatrix.M[3][3] - 1.0f) < 1e-4f);
		if (ValidLODCount <= 1 || IsOrthoGraphic) return 0;

		// 1. 바운딩 박스를 통해 바운딩 스피어 반지름 및 카메라와의 거리 계산
		const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
		const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
		const float SphereRadius = std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);

		const FVector Diff = Center - CameraPos;
		const float Dist = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);

		if (Dist <= 1e-4f) return 0;

		const float ProjectedRadius = (SphereRadius / Dist) * ProjMatrix.M[2][1];
		const float ScreenCoverage = ProjectedRadius; 

		static constexpr float Thresholds[] = { 0.05f, 0.03f, 0.01f, 0.008f };
		static constexpr int32 ThresholdCount = static_cast<int32>(sizeof(Thresholds) / sizeof(Thresholds[0]));

		const int32 MaxLOD = ValidLODCount - 1;
		for (int32 LOD = 0; LOD < MaxLOD; ++LOD)
		{
			float Threshold = (LOD < ThresholdCount) ? Thresholds[LOD] : 0.0f;
			if (ScreenCoverage >= Threshold)
				return LOD;
		}

		// 화면에 차지하는 비율이 가장 낮을 경우 최하위 LOD 반환
		return MaxLOD;
	}
}

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
                                    const FFrustum* ViewFrustum)
{
	ResetCullingStats();
	ResetDecalStats();

	if (!World) return;

	if (ViewFrustum != nullptr)
	{
		CollectWorldWithFrustum(World, *ViewFrustum, ShowFlags, ViewMode, RenderBus);
		return;
	}

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;

		if (!Actor || !Actor->IsVisible()) continue;

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{	
			if (Primitive != nullptr && Primitive->IsVisible())
			{
				++LastCullingStats.TotalVisiblePrimitiveCount;
			}
		}

		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus, World->GetWorldType());
	}

}

void FRenderCollector::ResetCullingStats()
{
	LastCullingStats = {};
}

void FRenderCollector::ResetDecalStats()
{
	LastDecalStats = {};
}

void FRenderCollector::CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
                                               EViewMode ViewMode, FRenderBus& RenderBus)
{
	VisiblePrimitiveScratch.clear();
	World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

	for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
	{
		if (Primitive == nullptr || UsesCameraDependentRenderBounds(Primitive))
		{
			continue;
		}

		++LastCullingStats.BVHPassedPrimitiveCount;
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType());
	}

	std::unordered_set<UPrimitiveComponent*> CollectedCameraDependentPrimitives;
	CollectedCameraDependentPrimitives.reserve(32);

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (Actor == nullptr || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive == nullptr || !Primitive->IsVisible())
			{
				continue;
			}

			++LastCullingStats.TotalVisiblePrimitiveCount;

			if (!UsesCameraDependentRenderBounds(Primitive))
			{
				if (Primitive->IsEnableCull())
					continue;
			}

			if (!CollectedCameraDependentPrimitives.insert(Primitive).second)
			{
				continue;
			}

			if (ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) == FFrustum::EFrustumIntersectResult::Outside)
			{
			}

			++LastCullingStats.FallbackPassedPrimitiveCount;
			CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType());
		}
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	bool bHasSelectionMask = false;
	for (AActor* Actor : SelectedActors)
	{
		bHasSelectionMask |= CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus);
	}

	if (bHasSelectionMask)
	{
		FRenderCommand PostProcessCmd = {};
		PostProcessCmd.Type = ERenderCommandType::PostProcessOutline;
		PostProcessCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
		PostProcessCmd.Constants.Outline.OutlineThicknessPixels = 5.0f;
		RenderBus.AddCommand(ERenderPass::PostProcessOutline, PostProcessCmd);
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Grid;
	Cmd.Constants.Grid.GridSpacing = GridSpacing;
	Cmd.Constants.Grid.GridHalfLineCount = GridHalfLineCount;
	Cmd.Constants.Grid.bOrthographic = bOrthographic;
	RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation)
{
	if (ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	FMeshBuffer* GizmoMesh = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
	FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
	bool bHolding = Gizmo->IsHolding();
	int32 SelectedAxis = Gizmo->GetSelectedAxis();

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = GizmoMesh;

		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = GizmoMesh->GetIndexBuffer().GetIndexCount();

		Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoInside;
			Cmd.BlendState = EBlendState::AlphaBlend;
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoOutside;
			Cmd.BlendState = EBlendState::Opaque;
		}
		Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd.Constants.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.Constants.Gizmo.bClicking = bHolding ? 1 : 0;
		Cmd.Constants.Gizmo.SelectedAxis = (SelectedAxis >= 0 && bIsActiveOperation) ? (uint32)SelectedAxis : 0xffffffffu;
		Cmd.Constants.Gizmo.HoveredAxisOpacity = 0.3f;
		return Cmd;
		};

	RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

	if (!bHolding)
	{
		RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, WorldType);
	}
}

bool FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	(void)ViewMode;
	if (!Actor->IsVisible()) return false;

	bool bHasSelectionMask = false;
	std::unordered_set<int32> SeenBVHNodeIndices;

	for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
	{
		if (!primitiveComponent->IsVisible()) continue;
		if (primitiveComponent->IsEditorOnly())
		{
			UWorld* World = Actor->GetWorld();
			if (World && World->GetWorldType() != EWorldType::Editor)
				continue;
		}

		FMeshBuffer* MeshBuffer = nullptr;
		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
		{
			auto* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
			MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh());
		}
		else
		{
			MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		}

		if (!MeshBuffer)
		{
			continue;
		}

		FRenderCommand BaseCmd{};
		BaseCmd.MeshBuffer = MeshBuffer;
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
		BaseCmd.SectionIndexStart = 0;
		BaseCmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			FMatrix WorldMatrix = TextComp->GetTextMatrix();

			FRenderCommand TextCmd = BaseCmd;
			BaseCmd.PerObjectConstants.Model = WorldMatrix;
			TextCmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix()};
			TextCmd.Type = ERenderCommandType::Font;
			TextCmd.PerObjectConstants.Color = TextComp->GetColor();
			TextCmd.Constants.Font.Text = &Text;
			TextCmd.Constants.Font.Font = Font;
			TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
			TextCmd.BlendState = EBlendState::AlphaBlend;
			TextCmd.DepthStencilState = EDepthStencilState::Default;
			RenderBus.AddCommand(ERenderPass::Font, TextCmd);
		}
		else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
		{
			BaseCmd.PerObjectConstants.Model = MakeViewSubUVSelectionMatrix(
				static_cast<USubUVComponent*>(primitiveComponent),
				RenderBus);
		}

		else if(primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Billboard)
		{
			BaseCmd.PerObjectConstants.Model = MakeViewBillboardMatrix(primitiveComponent, RenderBus);
		}

		if (!primitiveComponent->SupportsOutline()) continue;

		// Selection Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionMask;
		RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
		bHasSelectionMask = true;

		// TODO: 리팩토링 필요 (현재는 DecalComponent만 OBB를 그리도록 설정)
		UDecalComponent* DecalComp = dynamic_cast<UDecalComponent*>(primitiveComponent);
		if (DecalComp)
		{
			CollectOBBCommand(primitiveComponent, ShowFlags, RenderBus);
		}
		else
		{
			CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
		}

		CollectBVHInternalNodeAABBs(primitiveComponent, ShowFlags, RenderBus, SeenBVHNodeIndices);
	}

	return bHasSelectionMask;
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType)
{
	if (!Primitive->IsVisible()) return;
	if (Primitive->IsEditorOnly() && WorldType != EWorldType::Editor) return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();

	ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();
	auto ResolveSRV = [&](const FString& Path) -> ID3D11ShaderResourceView*
		{
			FMaterialResource* Res = FResourceManager::Get().FindTexture(Path);
			return (Res && Res->SRV) ? Res->SRV.Get() : DefaultSRV;
		};
	static const FMaterial EngineDefaultMaterial{};

	switch (PrimType)
	{
	case EPrimitiveType::EPT_StaticMesh:
	{
		if (!ShowFlags.bPrimitives) return;

		UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
		const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

		if (!StaticMesh || !StaticMesh->HasValidMeshData()) return;

		// 1. 카메라 정보 및 AABB 가져오기
        FVector CameraPos = RenderBus.GetCameraPosition();
        FMatrix ProjMatrix = RenderBus.GetProj();
        FAABB Bounds = StaticMeshComp->GetWorldAABB();
        const int32 ValidLODCount = StaticMesh->GetValidLODCount();

        // 2. LOD 레벨 계산
		int32 SelectedLOD = 0; // 기본값은 항상 원본(최고 화질)
        if (ShowFlags.bEnableLOD)
        {
            SelectedLOD = SelectLODLevel(CameraPos, Bounds, ProjMatrix, ValidLODCount);
        }

		FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, SelectedLOD);
        if (!MeshBuffer) return;

        const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
        const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

		for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
		{
			const FStaticMeshSection& Section = Sections[SectionIdx];

			FRenderCommand Cmd = {};
			Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.BlendState = EBlendState::Opaque;

			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;

			Cmd.Constants.StaticMesh.CameraWorldPos = RenderBus.GetCameraPosition();

			// 메테리얼 정보가 없을 시 디폴트 메테리얼을 사용합니다.
			const FMaterial* MtlData = &Cast<UMaterial>(StaticMeshComp->GetMaterial(SectionIdx))->MaterialData;

			if (!MtlData) MtlData = &EngineDefaultMaterial;
	
			Cmd.Constants.StaticMesh.AmbientColor  = MtlData->AmbientColor;
			Cmd.Constants.StaticMesh.DiffuseColor  = MtlData->DiffuseColor;
			Cmd.Constants.StaticMesh.SpecularColor = MtlData->SpecularColor;
			Cmd.Constants.StaticMesh.Shininess     = MtlData->Shininess;
			Cmd.Constants.StaticMesh.EmissiveColor = MtlData->EmissiveColor;

			Cmd.Constants.StaticMesh.ScrollX = StaticMeshComp->GetScroll().first;
			Cmd.Constants.StaticMesh.ScrollY = StaticMeshComp->GetScroll().second;

			// 와이어 프레임이 있는 경우 텍스쳐를 사용하지 않는 메테리얼에게 기본 텍스쳐를 강제 주입
			if (ViewMode == EViewMode::Wireframe)
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap =  1u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = 1u;
				Cmd.Constants.StaticMesh.DiffuseSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.AmbientSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.SpecularSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.BumpSRV = DefaultSRV;
			}
			else
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap = MtlData->bHasDiffuseTexture ? 1u : 0u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = MtlData->bHasSpecularTexture ? 1u : 0u;
				Cmd.Constants.StaticMesh.DiffuseSRV = MtlData->bHasDiffuseTexture ? ResolveSRV(MtlData->DiffuseTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.AmbientSRV = MtlData->bHasAmbientTexture ? ResolveSRV(MtlData->AmbientTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.SpecularSRV = MtlData->bHasSpecularTexture ? ResolveSRV(MtlData->SpecularTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.BumpSRV = MtlData->bHasBumpTexture ? ResolveSRV(MtlData->BumpTexPath) : DefaultSRV;
			}

			Cmd.Material = StaticMeshComp->GetMaterial(SectionIdx);

			RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
		}

		break;
	}

	case EPrimitiveType::EPT_Text:
	{
		if (!ShowFlags.bBillboardText) return;

		UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
		const FFontResource* Font = TextComp->GetFont();
		if (!Font || !Font->IsLoaded()) return;

		const FString& Text = TextComp->GetText();
		if (Text.empty()) return;

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Font;
		Cmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix(), TextComp->GetColor()};
		Cmd.Constants.Font.Text = &Text;
		Cmd.Constants.Font.Font = Font;
		Cmd.Constants.Font.Scale = TextComp->GetFontSize();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		
		RenderBus.AddCommand(ERenderPass::Font, Cmd);
		break;
	}

	case EPrimitiveType::EPT_SubUV:
	{
		USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		FRenderCommand Cmd = {};
		Cmd.PerObjectConstants = FPerObjectConstants{
			MakeViewBillboardMatrix(Primitive, RenderBus),
			FColor::White().ToVector4() };
		Cmd.Type = ERenderCommandType::SubUV;
		Cmd.Constants.SubUV.Particle = Particle;
		Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
		Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
		Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
		break;
	}

	case EPrimitiveType::EPT_Billboard:
	{
		UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Primitive);
		FMaterialResource* Sprite = BillboardComp->GetCachedSprite();

		ID3D11ShaderResourceView* SRV = (Sprite && Sprite->SRV)	? Sprite->SRV.Get() : FResourceManager::Get().GetDefaultWhiteSRV();

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Billboard;
		Cmd.PerObjectConstants = FPerObjectConstants{
			MakeViewBillboardMatrix(Primitive, RenderBus),
			FColor::White().ToVector4() };
		Cmd.Constants.Billboard.SRV = SRV;
		Cmd.Constants.Billboard.Width = BillboardComp->GetWidth();
		Cmd.Constants.Billboard.Height = BillboardComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::SubUV, Cmd);  // SubUV 패스 재사용
		break;
	}
	
	case EPrimitiveType::EPT_Decal:
	{
		if (!ShowFlags.bDecals) return;

		FScopeCycleCounter RenderDecalScope({});

		UDecalComponent* DecalComp = static_cast<UDecalComponent*>(Primitive);
		const UMaterial* MtlData = Cast<UMaterial>(DecalComp->GetMaterial());

		if (!MtlData)
		{
			return;
		}

		UWorld* World = DecalComp->GetOwner() ? DecalComp->GetOwner()->GetWorld() : nullptr;

		FOBB DecalOBB = FOBB::FromAABB(DecalComp->GetWorldAABB(), DecalComp->GetWorldMatrix());

		TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
		World->GetSpatialIndex().OBBQueryPrimitives(DecalOBB, VisiblePrimitiveScratch, OBBQueryScratch);

		for (UPrimitiveComponent* Prim : VisiblePrimitiveScratch)
		{
			if (Prim->GetPrimitiveType() != EPrimitiveType::EPT_StaticMesh) continue;

			UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Prim);
			const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();

			if (!StaticMesh || !StaticMesh->HasValidMeshData()) return;

			// 1. 카메라 정보 및 AABB 가져오기
			FVector CameraPos = RenderBus.GetCameraPosition();
			FMatrix ProjMatrix = RenderBus.GetProj();
			FAABB Bounds = StaticMeshComp->GetWorldAABB();
			const int32 ValidLODCount = StaticMesh->GetValidLODCount();

			int32 SelectedLOD = 0; // 기본값은 항상 원본(최고 화질)
			if (ShowFlags.bEnableLOD)
			{
				SelectedLOD = SelectLODLevel(CameraPos, Bounds, ProjMatrix, ValidLODCount);
			}

			FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh, SelectedLOD);
			if (!MeshBuffer) return;

			const FStaticMesh* MeshData = StaticMesh->GetMeshData(SelectedLOD);
			const TArray<FStaticMeshSection>& Sections = MeshData->Sections;

			for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
			{
				const FStaticMeshSection& Section = Sections[SectionIdx];

				FRenderCommand Cmd = {};
				Cmd.Type = ERenderCommandType::Decal;
				Cmd.PerObjectConstants = FPerObjectConstants{ Prim->GetWorldMatrix(), FColor::White().ToVector4() };
				Cmd.MeshBuffer = MeshBuffer;

				Cmd.Constants.Decal.InvDecalWorld = DecalComp->GetDecalMatrix().GetInverse();
				Cmd.Constants.Decal.ColorTint = DecalComp->GetDecalColor().ToVector4();
				Cmd.Constants.Decal.FadeAlpha = 1.0f;

				Cmd.Constants.Decal.DiffuseSRV = ResolveSRV(MtlData->MaterialData.DiffuseTexPath);
				Cmd.BlendState = EBlendState::AlphaBlend;
				Cmd.DepthStencilState = EDepthStencilState::Default;

				Cmd.SectionIndexStart = Section.StartIndex;
				Cmd.SectionIndexCount = Section.IndexCount;

				RenderBus.AddCommand(ERenderPass::Decal, Cmd);
			}
		}

		LastDecalStats.TotalDecalCount += 1;
		LastDecalStats.CollectTimeMS += static_cast<int32>(RenderDecalScope.Finish());
		break;
	}
	
    case EPrimitiveType::EPT_FOG:
    {
        UHeightFogComponent* HeightFogComp = static_cast<UHeightFogComponent*>(Primitive);

        FRenderCommand Cmd = {};
        Cmd.Type = ERenderCommandType::Primitive;
        Cmd.Constants.Fog.FogDensity = HeightFogComp->GetFogDensity();
        Cmd.Constants.Fog.FogColor = HeightFogComp->GetFogInscatteringColor();
        Cmd.Constants.Fog.HeightFalloff = HeightFogComp->GetHeightFalloff();
        Cmd.Constants.Fog.FogHeight = HeightFogComp->GetFogHeight();
        Cmd.Constants.Fog.FogStartDistance = HeightFogComp->GetFogStartDistance();
        Cmd.Constants.Fog.FogMaxOpacity = HeightFogComp->GetFogMaxOpacity();
        Cmd.Constants.Fog.FogCutoffDistance = HeightFogComp->GetFogCutoffDistance();
        Cmd.BlendState = EBlendState::AlphaBlend;
        Cmd.DepthStencilState = EDepthStencilState::Default;

        RenderBus.AddCommand(ERenderPass::Fog, Cmd);
        break;
    }
    case EPrimitiveType::EPT_Fireball:
    {
		UFireballComponent* FireballComp = static_cast<UFireballComponent*>(Primitive);

		FLightData LightData = {};
		LightData.Intensity = FireballComp->GetIntensity();
		LightData.Radius	= FireballComp->GetRadius();
		LightData.RadiusFalloff = FireballComp->GetRadiusFallOff();
		LightData.WorldPos  = FireballComp->GetWorldLocation();

		FColor Color = FireballComp->GetLinearColor();
		LightData.Color.X = Color.R;
		LightData.Color.Y = Color.G;
		LightData.Color.Z = Color.B;
		RenderBus.AddLight(LightData);
		break;
	}
	default:
		if (PrimType == EPrimitiveType::EPT_TransGizmo || PrimType == EPrimitiveType::EPT_RotGizmo || PrimType == EPrimitiveType::EPT_ScaleGizmo)
		{
			return;
		}
		return;
	}
}

void FRenderCollector::CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                                   FRenderBus& RenderBus, std::unordered_set<int32>& SeenNodeIndices)
{
	if (!ShowFlags.bBoundingVolume || !ShowFlags.bBVHBoundingVolume || PrimitiveComponent == nullptr)
	{
		return;
	}

	AActor* Owner = PrimitiveComponent->GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FWorldSpatialIndex& SpatialIndex = World->GetSpatialIndex();
	const int32 ObjectIndex = SpatialIndex.FindObjectIndex(PrimitiveComponent);
	if (ObjectIndex == FBVH::INDEX_NONE)
	{
		return;
	}

	const FBVH& BVH = SpatialIndex.GetBVH();
	const TArray<int32>& ObjectToLeafNode = BVH.GetObjectToLeafNode();
	if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
	{
		return;
	}

	const int32 LeafNodeIndex = ObjectToLeafNode[ObjectIndex];
	if (LeafNodeIndex == FBVH::INDEX_NONE)
	{
		return;
	}

	const TArray<FBVH::FNode>& Nodes = BVH.GetNodes();
	if (LeafNodeIndex < 0 || LeafNodeIndex >= static_cast<int32>(Nodes.size()))
	{
		return;
	}

	TArray<int32> PathToRoot;
	PathToRoot.reserve(16);

	int32 CurrentNodeIndex = Nodes[LeafNodeIndex].Parent;
	while (CurrentNodeIndex != FBVH::INDEX_NONE)
	{
		if (CurrentNodeIndex < 0 || CurrentNodeIndex >= static_cast<int32>(Nodes.size()))
		{
			break;
		}

		PathToRoot.push_back(CurrentNodeIndex);
		CurrentNodeIndex = Nodes[CurrentNodeIndex].Parent;
	}

	for (int32 PathIndex = 0; PathIndex < static_cast<int32>(PathToRoot.size()); ++PathIndex)
	{
		const int32 NodeIndex = PathToRoot[PathIndex];
		if (!SeenNodeIndices.insert(NodeIndex).second)
		{
			continue;
		}

		const FBVH::FNode& Node = Nodes[NodeIndex];
		if (Node.IsLeaf())
		{
			continue;
		}

		const FColor Color = MakeBVHInternalNodeColor(PathIndex, static_cast<int32>(PathToRoot.size()));
		CollectAABBCommand(Node.Bounds, Color, RenderBus);
	}
}

void FRenderCollector::CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus)
{
	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox;
	AABBCmd.Constants.AABB.Min = Box.Min;
	AABBCmd.Constants.AABB.Max = Box.Max;
	AABBCmd.Constants.AABB.Color = Color;
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (!ShowFlags.bBoundingVolume) return;

	const FAABB Box = BuildRenderAABB(PrimitiveComponent, RenderBus);
	CollectAABBCommand(Box, FColor::White(), RenderBus);
}

void FRenderCollector::CollectOBBCommand(const FOBB& Box, const FColor& Color, FRenderBus& RenderBus)
{
	FRenderCommand OBBCmd = {};
	OBBCmd.Type = ERenderCommandType::DebugOBB;
	OBBCmd.Constants.OBB.Center = Box.Center;
	OBBCmd.Constants.OBB.Extents = Box.Extents;
	OBBCmd.Constants.OBB.Rotation = Box.Rotation.ToMatrix();
	OBBCmd.Constants.OBB.Color = Color;
	RenderBus.AddCommand(ERenderPass::Editor, OBBCmd);
}

void FRenderCollector::CollectOBBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (!ShowFlags.bBoundingVolume) return;

	const FAABB AABB = PrimitiveComponent->GetWorldAABB();
	const FOBB Box = FOBB::FromAABB(AABB, PrimitiveComponent->GetWorldMatrix());
	CollectOBBCommand(Box, FColor::Green(), RenderBus);
}
