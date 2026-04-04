#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/Math/ConvexVolume.h"
#include "Engine/Component/CameraComponent.h"
#include <algorithm>

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

void UWorld::UpdateVisibleActors()
{
	VisiblePrimitives.clear();
	VisibleActors.clear();

	// 모든 프록시를 컬링 상태로 초기화
	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (Proxy) Proxy->bFrustumCulled = true;
	}

	FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();

 	Partition.QueryFrustumAllPrimitive(ConvexVolume, VisiblePrimitives);

	// 보이는 primitive의 프록시만 컬링 해제
	for (UPrimitiveComponent* Primitive : VisiblePrimitives)
	{
		if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
			Proxy->bFrustumCulled = false;

		if (AActor* Owner = Primitive->GetOwner())
		{
			VisibleActors.push_back(Owner);
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
	UpdateVisibleActors();
	DebugDrawQueue.Tick(DeltaTime);

	for (AActor* Actor : VisibleActors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
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
