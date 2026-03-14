#include "World.h"
#include "FObjectFactory.h"
#include "UPrimitiveComponent.h"
#include "algorithm"

UScene* UWorld::activeScene;

UWorld::UWorld() {}

UWorld::~UWorld() {}

UScene* UWorld::GetActiveScene()
{
	return activeScene;
}

void UWorld::SetActiveScene(UScene* scene)
{
	activeScene = scene;
}

void UWorld::RemoveSceneComponent(USceneComponent* toDespawn)
{
	for (auto it = activeScene->SceneComponents.begin(); it != activeScene->SceneComponents.end(); it++)
	{
		if ((*it) == toDespawn)
		{
			activeScene->SceneComponents.erase(it);
			break;
		}
	}
	
	auto primToDespawn = Cast<UPrimitiveComponent>(toDespawn);
	if (primToDespawn != nullptr)
	{
		auto prims = &(activeScene->PrimitiveContainer);
		for (auto it = prims->begin(); it != prims->end(); it++)
		{
			if ((*it) == primToDespawn)
			{
				prims->erase(it);
				break;
			}
		}
	}
}

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}