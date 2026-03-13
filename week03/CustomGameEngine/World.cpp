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

template <typename T>
T* UWorld::AddSceneComponent()
{
	// TODO: 액터 개념 도입되면 USceneComponent 대신 UActor로 교체
	static_assert(std::is_base_of<USceneComponent, T>::value, "T must derive from USceneComponent");

	int uuid = activeScene->NextUUID;
	T* instance = FObjectFactory::ConstructObject(uuid);

	activeScene->Primitives.push_back(static_cast<USceneComponent*>(instance));

	activeScene->NextUUID += 1;
	return instance;
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