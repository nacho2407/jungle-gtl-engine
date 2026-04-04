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

	delete Octree;
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
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim || !Prim->IsVisible()) continue;
        Prim->UpdateWorldMatrix();
        Octree->Insert(Prim);
    }
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim) continue;
        Octree->Remove(Prim);
    }
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim) continue;

        if (Prim->IsVisible())
        {
			Prim->UpdateWorldMatrix();
			Octree->MarkDirty(Prim);
        }
    }
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

 	Octree->QueryFrustum(ConvexVolume, VisiblePrimitives);

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
	delete Octree;
	Octree = new FOctree(FBoundingBox(FVector(-40, -40, -40), FVector(40, 40, 40)), 0);
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
    if (Octree)
		Octree->FlushDirty();
	UpdateVisibleActors();
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
