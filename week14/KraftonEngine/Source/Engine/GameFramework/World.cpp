#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/Math/ConvexVolume.h"
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

	MarkPickingBVHDirty();
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
	MarkPickingBVHDirty();
}

void UWorld::MarkPickingBVHDirty()
{
	PickingBVH.MarkDirty();
}

void UWorld::BuildPickingBVHNow() const
{
	PickingBVH.BuildNow(Actors);
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	PickingBVH.EnsureBuilt(Actors);
	return PickingBVH.Raycast(Ray, OutHitResult, OutActor);
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

void UWorld::UpdateVisibleProxies()
{
	VisiblePrimitives.clear();
	VisibleProxies.clear();

	{
		SCOPE_STAT_CAT("FrustumCulling", "1_WorldTick");
		FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();
 		Partition.QueryFrustumAllPrimitive(ConvexVolume, VisiblePrimitives);
	}

	{
		SCOPE_STAT_CAT("BuildVisibleProxies", "1_WorldTick");

		// NeverCull 프록시 (Gizmo 등) — 항상 visible
		for (FPrimitiveSceneProxy* Proxy : Scene.GetNeverCullProxies())
			VisibleProxies.push_back(Proxy);

		// Frustum 쿼리 결과 → VisibleProxies 구축
		for (UPrimitiveComponent* Primitive : VisiblePrimitives)
		{
			if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
				VisibleProxies.push_back(Proxy);
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
	UpdateVisibleProxies();
	DebugDrawQueue.Tick(DeltaTime);

	// bNeedsTick인 Actor만 Tick (visibility 무관)
	for (AActor* Actor : Actors)
	{
		if (Actor && Actor->bNeedsTick)
			Actor->Tick(DeltaTime);
	}
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
	MarkPickingBVHDirty();
}
