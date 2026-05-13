#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Engine/Geometry/Frustum.h"
#include "Engine/GameFramework/PrimitiveActors.h"

namespace
{
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
		const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(Primitive);
		return UBillboardComponent::MakeBillboardWorldMatrix(
			WorldMatrix.GetOrigin(),
			Billboard->GetBillboardWorldScale(),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}

	FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
	{
		const FVector WorldScale = SubUVComp->GetBillboardWorldScale();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			SubUVComp->GetWorldLocation(),
			FVector(
				WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
				SubUVComp->GetWidth() * WorldScale.Y * 0.5f,
				SubUVComp->GetHeight() * WorldScale.Z * 0.5f),
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
}

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
                                    const FFrustum* ViewFrustum, bool bIncludeEditorOnlyPrimitives)
{
	ResetCullingStats();
	ResetDecalStats();
	ResetLightStats();

	if (!World) return;

	if (ViewFrustum != nullptr)
	{
		CollectWorldWithFrustum(World, *ViewFrustum, ShowFlags, ViewMode, RenderBus, bIncludeEditorOnlyPrimitives);
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

		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
	}

}

void FRenderCollector::ResetCullingStats()
{
	LastCullingStats = {};
}

void FRenderCollector::ResetDecalStats()
{
	DecalCommandBuilder.Reset();
}

void FRenderCollector::ResetLightStats()
{
	LightRenderCollector.Reset();
}

void FRenderCollector::CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
                                               EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	VisiblePrimitiveScratch.clear();
	World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

	for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
	{
		if (Primitive == nullptr || UsesCameraDependentRenderBounds(Primitive) || !Primitive->IsEnableCull())
		{
			continue;
		}

		++LastCullingStats.BVHPassedPrimitiveCount;
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
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

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (ULightComponentBase* Light = Cast<ULightComponentBase>(Comp))
			{
				CollectLight(Light, RenderBus);
			}
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive == nullptr || !Primitive->IsVisible())
			{
				continue;
			}

			++LastCullingStats.TotalVisiblePrimitiveCount;

			const bool bIsCameraDependent = UsesCameraDependentRenderBounds(Primitive);
			const bool bIsUncullable = !Primitive->IsEnableCull();

			if (!bIsCameraDependent && !bIsUncullable)
			{
				continue;
			}

			if (!CollectedCameraDependentPrimitives.insert(Primitive).second)
			{
				continue;
			}

			if (bIsCameraDependent && !bIsUncullable &&
				ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) == FFrustum::EFrustumIntersectResult::Outside)
			{
				continue;
			}

			++LastCullingStats.FallbackPassedPrimitiveCount;
			CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
		}
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags,
                                        EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	EditorOverlayCollector.CollectSelection(
		SelectedActors,
		ShowFlags,
		ViewMode,
		RenderBus,
		MeshBufferManager,
		bIncludeEditorOnlyPrimitives);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic)
{
	EditorOverlayCollector.CollectGrid(GridSpacing, GridHalfLineCount, RenderBus, bOrthographic);
}

void FRenderCollector::CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus)
{
	EditorOverlayCollector.CollectSkeletonBones(SkComp, RenderBus);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation)
{
	EditorOverlayCollector.CollectGizmo(Gizmo, ShowFlags, RenderBus, MeshBufferManager, bIsActiveOperation);
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
                                        EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
	}

	if (Actor->IsA<ADecalSpotLightActor>())
	{
		ADecalSpotLightActor* SpotlightActor = Cast<ADecalSpotLightActor>(Actor);

	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                            FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (!Primitive->IsVisible()) return;
	if (!bIncludeEditorOnlyPrimitives && Primitive->IsEditorOnly() && WorldType != EWorldType::Editor) return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();

	if (PrimType != EPrimitiveType::EPT_Decal)
	{
		PrimitiveDrawCommandBuilder.CollectPrimitive(Primitive, ShowFlags, ViewMode, RenderBus, MeshBufferManager);
		return;
	}

	switch (PrimType)
	{
	case EPrimitiveType::EPT_Decal:
	{
		DecalCommandBuilder.CollectDecal(Primitive, ShowFlags, RenderBus, MeshBufferManager, OBBQueryScratch);
		break;
	}
	default:
		return;
	}
}

void FRenderCollector::CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus)
{
	LightRenderCollector.CollectLight(Light, RenderBus);
}
