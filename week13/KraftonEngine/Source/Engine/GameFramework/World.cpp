#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Render/Culling/ConvexVolume.h"
#include "Engine/Component/CameraComponent.h"
#include <algorithm>
#include "Profiling/Stats.h"

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::~UWorld()
{
	if (!Actors.empty())
	{
		EndPlay();
	}
}

void UWorld::DestroyActor(AActor* Actor)
{
	// remove and clean up
	if (!Actor) return;
	Actor->EndPlay();
	// Remove from actor list
	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
		Actors.erase(it);

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->SetWorld(this);
	Actors.push_back(Actor);

	InsertActorToOctree(Actor);
	MarkWorldPrimitivePickingBVHDirty();

}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(Actors);
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	//WorldPrimitivePickingBVH.EnsureBuilt(Actors); //혹시라도 BVH 트리가 업데이트 되지 않았다면 업데이트
	return WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor);
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);

}

// ── LOD 거리 임계값 (제곱) ──
static constexpr float LOD1_DIST_SQ = 15.0f * 15.0f;   // LOD0→LOD1
static constexpr float LOD2_DIST_SQ = 25.0f * 25.0f;   // LOD1→LOD2
static constexpr float LOD3_DIST_SQ = 40.0f * 40.0f;   // LOD2→LOD3
// 히스테리시스: 더 상세한 LOD로 복귀하려면 10% 더 가까워야 함
static constexpr float LOD1_BACK_SQ = 13.5f * 13.5f;    // LOD1→LOD0
static constexpr float LOD2_BACK_SQ = 22.5f * 22.5f;    // LOD2→LOD1
static constexpr float LOD3_BACK_SQ = 36.0f * 36.0f;    // LOD3→LOD2
static constexpr uint32 LOD_UPDATE_SLICE_COUNT = 4;
static constexpr uint32 LOD_STAGGER_MIN_VISIBLE = 2048;
static constexpr float LOD_FULL_UPDATE_CAMERA_MOVE_SQ = 1.0f * 1.0f;
static constexpr float LOD_FULL_UPDATE_CAMERA_ROTATION_DOT = 0.9986295f; // about 3 degrees

static_assert((LOD_UPDATE_SLICE_COUNT& (LOD_UPDATE_SLICE_COUNT - 1)) == 0,
	"LOD_UPDATE_SLICE_COUNT must be a power of two.");

static uint32 SelectLOD(uint32 CurLOD, float DistSq)
{
	uint32 LOD = CurLOD;

	// 멀어질 때 — 덜 상세한 LOD로
	if (CurLOD == 0 && DistSq > LOD1_DIST_SQ)  LOD = 1;
	if (CurLOD <= 1 && DistSq > LOD2_DIST_SQ)  LOD = 2;
	if (CurLOD <= 2 && DistSq > LOD3_DIST_SQ)  LOD = 3;
	// 가까워질 때 — 더 상세한 LOD로 (히스테리시스)
	if (CurLOD == 3 && DistSq < LOD3_BACK_SQ)   LOD = 2;
	if (CurLOD >= 2 && DistSq < LOD2_BACK_SQ)   LOD = 1;
	if (CurLOD >= 1 && DistSq < LOD1_BACK_SQ)   LOD = 0;

	return LOD;
}

static float DistanceSquared(const FVector& A, const FVector& B)
{
	const FVector D = A - B;
	return D.X * D.X + D.Y * D.Y + D.Z * D.Z;
}

void UWorld::UpdateVisibleProxies()
{
	VisibleProxies.clear();

	// HEAD: capacity 예약으로 재할당 방지
	const uint32 ExpectedProxyCount = Scene.GetProxyCount();
	if (VisibleProxies.capacity() < ExpectedProxyCount)
	{
		VisibleProxies.reserve(ExpectedProxyCount);
	}

	const uint32 ExpectedVisibleProxyCount =
		ExpectedProxyCount + static_cast<uint32>(Scene.GetNeverCullProxies().size());
	if (VisibleProxies.capacity() < ExpectedVisibleProxyCount)
	{
		VisibleProxies.reserve(ExpectedVisibleProxyCount);
	}

	const FVector CameraPos = ActiveCamera->GetWorldLocation();
	const FVector CameraForward = ActiveCamera->GetForwardVector();

	{
		SCOPE_STAT_CAT("FrustumCulling", "1_WorldTick");
		FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();
		// Direct proxy output — skips Component→GetSceneProxy() indirection in BuildVisibleProxies
		Partition.QueryFrustumAllProxies(ConvexVolume, VisibleProxies);
	}

	{
		SCOPE_STAT_CAT("BuildVisibleProxies", "1_WorldTick");

		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
			VisibleProxies.push_back(Proxy);

		LOD_STATS_RESET();
		const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
		const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
		const bool bShouldStaggerLOD = VisibleProxies.size() >= LOD_STAGGER_MIN_VISIBLE;

		const bool bForceFullLODRefresh =
			!bShouldStaggerLOD
			|| LastLODUpdateCamera != ActiveCamera
			|| !bHasLastFullLODUpdateCameraPos
			|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
			|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

		if (bForceFullLODRefresh)
		{
			LastLODUpdateCamera = ActiveCamera;
			LastFullLODUpdateCameraPos = CameraPos;
			LastFullLODUpdateCameraForward = CameraForward;
			bHasLastFullLODUpdateCameraPos = true;
		}

		// main: QueryFrustumAllProxies가 이미 VisibleProxies를 채웠으므로 직접 순회
		// HEAD: LOD 스태거 최적화 유지 (프록시 수가 많을 때 슬라이스별 갱신)
		const uint32 Count = static_cast<uint32>(VisibleProxies.size());
		for (uint32 i = 0; i < Count; i++)
		{
			FPrimitiveSceneProxy* Proxy = VisibleProxies[i];
			if (!Proxy) continue;

			const bool bRefreshLOD =
				bForceFullLODRefresh
				|| Proxy->LastLODUpdateFrame == UINT32_MAX
				|| ((Proxy->ProxyId & (LOD_UPDATE_SLICE_COUNT - 1)) == LODUpdateSlice);

			if (bRefreshLOD)
			{
				const FVector& ProxyPos = Proxy->CachedWorldPos;
				const float DistSq = DistanceSquared(CameraPos, ProxyPos);
				Proxy->UpdateLOD(SelectLOD(Proxy->CurrentLOD, DistSq));
				Proxy->LastLODUpdateFrame = LODUpdateFrame;
			}

			LOD_STATS_RECORD(Proxy->CurrentLOD);
		}
	}
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox(FVector(-100, -100, -100), FVector(100, 100, 100)));
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void UWorld::Tick(float DeltaTime)
{
	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	UpdateVisibleProxies();

#if _DEBUG
	DebugDrawQueue.Tick(DeltaTime);
#endif

#ifndef FPS_OPTIMIZATION
	// 유효하게 돌아가는 로직이 billboardcomponent 뿐인 것에 비해 오버헤드가 꽤 커서 이번 기간동안 주석
	for (AActor* Actor : Actors)
	{
		if (Actor && Actor->bNeedsTick)
			Actor->Tick(DeltaTime);
	}
#endif
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
	MarkWorldPrimitivePickingBVHDirty();
}