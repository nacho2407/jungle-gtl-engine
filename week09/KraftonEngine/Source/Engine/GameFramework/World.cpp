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

	if (bOcclusionBVHBuilt)
		OcclusionBVH.RemoveActor(Actor);

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

	if (bOcclusionBVHBuilt)
		OcclusionBVH.AddActor(Actor);
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
	WorldPrimitivePickingBVH.EnsureBuilt(Actors);
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

	if (bOcclusionBVHBuilt)
		OcclusionBVH.MarkDirty(Actor);
}

// ── Occlusion BVH ──
void UWorld::BuildOcclusionBVH()
{
	OcclusionBVH.Build(Actors);
	bOcclusionBVHBuilt = true;
}

// ── LOD 거리 임계값 (제곱) ──
static constexpr float LOD1_DIST_SQ = 15.0f * 15.0f;   // LOD0→LOD1
static constexpr float LOD2_DIST_SQ = 25.0f * 25.0f;   // LOD1→LOD2
static constexpr float LOD3_DIST_SQ = 40.0f * 40.0f;   // LOD2→LOD3
// 히스테리시스: 더 상세한 LOD로 복귀하려면 10% 더 가까워야 함
static constexpr float LOD1_BACK_SQ = 13.5f * 13.5f;    // LOD1→LOD0
static constexpr float LOD2_BACK_SQ = 22.5f * 22.5f;    // LOD2→LOD1
static constexpr float LOD3_BACK_SQ = 36.0f * 36.0f;    // LOD3→LOD2

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
	VisiblePrimitives.clear();
	VisibleProxies.clear();

	const FMatrix ViewProj = ActiveCamera->GetViewProjectionMatrix();
	const FVector CameraPos = ActiveCamera->GetWorldLocation();

	{
		SCOPE_STAT_CAT("FrustumOcclusionCulling", "1_WorldTick");
		FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();
		OcclusionCulling.Clear();

		if (bOcclusionBVHBuilt)
		{
			// BVH 기반: frustum + occlusion 통합 쿼리
			OcclusionBVH.QueryFrustumOcclusion(
				ConvexVolume, OcclusionCulling, ViewProj, CameraPos, VisiblePrimitives);
		}
		else
		{
			// BVH 미빌드 시 기존 Octree frustum 폴백
			Partition.QueryFrustumAllPrimitive(ConvexVolume, VisiblePrimitives);
		}
	}

	{
		SCOPE_STAT_CAT("BuildVisibleProxies", "1_WorldTick");

		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
			VisibleProxies.push_back(Proxy);

		LOD_STATS_RESET();

		for (UPrimitiveComponent* Primitive : VisiblePrimitives)
		{
			if (!Primitive) continue;
			if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
			{
				const FVector& ProxyPos = Proxy->CachedWorldPos;
				const float DistSq = DistanceSquared(CameraPos, ProxyPos);

				Proxy->UpdateLOD(SelectLOD(Proxy->CurrentLOD, DistSq));
				LOD_STATS_RECORD(Proxy->CurrentLOD);

				VisibleProxies.push_back(Proxy);
			}
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
	Partition.FlushPrimitive();

	// Occlusion BVH dirty 처리
	if (bOcclusionBVHBuilt)
		OcclusionBVH.Rebuild();

	UpdateVisibleProxies();
	DebugDrawQueue.Tick(DeltaTime);

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
	bOcclusionBVHBuilt = false;
	MarkWorldPrimitivePickingBVHDirty();
}
