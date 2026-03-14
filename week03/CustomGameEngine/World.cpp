#include "World.h"
#include "FObjectFactory.h"
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
	for (auto it = activeScene->Primitives.begin(); it != activeScene->Primitives.end(); it++)
	{
		if ((*it) == toDespawn)
		{
			activeScene->Primitives.erase(it);
			return;
		}
	}
}

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}