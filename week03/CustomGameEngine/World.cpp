#include "World.h"
#include "ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "EngineStatics.h"
#include "SerializeHelper.h"

UScene* UWorld::activeScene;
UScene* UWorld::permanentScene;

UWorld::UWorld() 
{
	permanentScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
}

UWorld::~UWorld() {
	UnloadScene();

	// permanentScene 정리
	FLevel::GetInstance().Clear();
	for (auto it = permanentScene->SceneComponents.begin(); it != permanentScene->SceneComponents.end(); it++)
	{
		delete *it;
	}
	permanentScene->SceneComponents.clear();
}

UScene* UWorld::GetActiveScene()
{
	return activeScene;
}

void UWorld::SetActiveScene(UScene* scene)
{
	activeScene = scene;
}

UScene* UWorld::NewScene()
{
	if (GetActiveScene() != nullptr)
		UnloadScene();

	UScene* newScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
	SetActiveScene(newScene);
	UEngineStatics::NextUUID = 0;

	UE_LOG("Created new scene\n");
	return GetActiveScene();
}

void UWorld::SaveScene(FString sceneName)
{
	UScene* activeScene = GetWorld().GetActiveScene();
	activeScene->NextUUID = UEngineStatics::NextUUID;

	SerializeHelper::SaveAsJson(activeScene, sceneName + ".Scene");
	return;
}

UScene* UWorld::LoadScene(FString sceneName)
{
	const FString filePath = sceneName + ".Scene";
	
	UScene* loadedScene = SerializeHelper::LoadFromJson<UScene>(filePath);

	if (loadedScene != nullptr)
	{
		if (GetActiveScene() != nullptr)
			UnloadScene();
		SetActiveScene(loadedScene);
		UEngineStatics::NextUUID = loadedScene->NextUUID;
	}

	return loadedScene;
}

void UWorld::UnloadScene()
{
	UScene* curScene = GetWorld().GetActiveScene();

	// loop 내에서의 삽입/삭제로 인한 오류 방지
	auto deleteList = curScene->SceneComponents;
	for (auto obj : deleteList)
	{
		RemoveSceneComponent(obj);
		delete obj;
	}
	delete GetActiveScene();
	SetActiveScene(nullptr);
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

void UWorld::Update(float deltaTime)
{
	permanentScene->Update(deltaTime);
	// TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	activeScene->Update(deltaTime);
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