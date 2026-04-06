#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Render/Culling/ConvexVolume.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Pipeline/LODContext.h"
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

// LOD 상수 및 SelectLOD는 LODContext.h에 정의

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

	{
		SCOPE_STAT_CAT("FrustumCulling", "1_WorldTick");
		FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();
		Partition.QueryFrustumAllProxies(ConvexVolume, VisibleProxies);
	}

	// NeverCull 프록시 추가 (LOD 갱신은 Collect 단계에서 병합 처리)
	for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
		VisibleProxies.push_back(Proxy);
}

FLODUpdateContext UWorld::PrepareLODContext()
{
	if (!ActiveCamera) return {};

	const FVector CameraPos = ActiveCamera->GetWorldLocation();
	const FVector CameraForward = ActiveCamera->GetForwardVector();

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

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
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
