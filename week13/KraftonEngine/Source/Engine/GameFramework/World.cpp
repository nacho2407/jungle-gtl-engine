#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"

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

	RemoveActorToOctree(Actor);
	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::InitWorld()
{
	 Octree = new FOctree(FBoundingBox(FVector(-100, -100, -100), FVector(100, 100, 100)), 0);
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

        Octree->Remove(Prim);
        Prim->UpdateWorldMatrix();

        if (Prim->IsVisible())
        {
            Octree->Insert(Prim);
        }
    }
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

	for (AActor* Actor : Actors)
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
}
