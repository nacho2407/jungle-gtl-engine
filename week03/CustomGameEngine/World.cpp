#include "World.h"
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"

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
	// Remove render object first
	auto primToDespawn = Cast<UPrimitiveComponent>(toDespawn);
	if (primToDespawn != nullptr)
	{
		FLevel::GetInstance().RemoveRenderObject(primToDespawn->GetRenderObject());
	}

	for (auto it = activeScene->SceneComponents.begin(); it != activeScene->SceneComponents.end(); it++)
	{
		if ((*it) == toDespawn)
		{
			activeScene->SceneComponents.erase(it);
			break;
		}
	}
}

void UWorld::OnBeforeRender() const
{
	// TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	auto sceneComponents = activeScene->SceneComponents;
	for (USceneComponent* s : sceneComponents)
	{
		UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(s);
		if (primitive != nullptr)
		{
			if (primitive->GetRenderObject()->bIsDirty)
				primitive->ResolveRenderStateDirty();
		}
	}
}

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}