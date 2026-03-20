#include "SceneManager.h"

class UEngineStatics;

SceneManager::SceneManager()
{
	PermanentScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
}

SceneManager::~SceneManager()
{
	// permanentScene 정리
	for (auto it = PermanentScene->SceneComponents.begin(); it != PermanentScene->SceneComponents.end(); it++)
	{
		delete* it;
	}
	PermanentScene->SceneComponents.clear();
	delete PermanentScene;
}


UScene* SceneManager::GetActiveScene()
{
	return ActiveScene;
}

void SceneManager::SetActiveScene(UScene* scene)
{
	ActiveScene = scene;
}

UScene* SceneManager::GetPermanentScene()
{
	return PermanentScene;
}

void SceneManager::SetPermanentScene(UScene* scene)
{
	PermanentScene = scene;
}

UScene* SceneManager::NewScene()
{
	if (GetActiveScene() != nullptr)
		UnloadScene();

	UScene* newScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
	SetActiveScene(newScene);
	UEngineStatics::NextUUID = 0;

	UE_LOG("Created new scene\n");
	return GetActiveScene();
}

void SceneManager::SaveScene(FString sceneName)
{
	UScene* activeScene = GetActiveScene();
	activeScene->NextUUID = UEngineStatics::NextUUID;

	SerializeHelper::SaveAsJson(activeScene, sceneName + ".Scene");
	return;
}

UScene* SceneManager::LoadScene(FString sceneName)
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

void SceneManager::UnloadScene()
{
	UScene* curScene = GetActiveScene();
	if (curScene == nullptr)
	{
		return;
	}
	// loop 내에서의 삽입/삭제로 인한 오류 방지
	auto deleteList = curScene->SceneComponents;
	for (auto obj : deleteList)
	{
		RemoveSceneComponent(obj);
	}
	delete GetActiveScene();
	SetActiveScene(nullptr);
}

void SceneManager::RemoveSceneComponent(USceneComponent* toDespawn)
{
	// Remove render object first
	auto primToDespawn = Cast<UPrimitiveComponent>(toDespawn);
	if (primToDespawn != nullptr)
	{
		FLevel::GetInstance().UnregisterRenderObject(primToDespawn->GetRenderObject());
	}

	for (auto it = ActiveScene->SceneComponents.begin(); it != ActiveScene->SceneComponents.end(); it++)
	{
		if ((*it) == toDespawn)
		{
			delete toDespawn;
			ActiveScene->SceneComponents.erase(it);
			break;
		}
	}
}
