#include "Prefab.h"

FPrefab::~FPrefab()
{
	UObjectManager::Get().DestroyObject(Actor);
	delete ActorImage;
}
