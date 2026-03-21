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


TArray<UScene*> SceneManager::GetLoadedScenes()
{
	return LoadedScenes;
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
	UScene* newScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
	newScene->Name = FName("NewScene_" + std::to_string((int32)LoadedScenes.size()));
	SetActiveScene(newScene);
	LoadedScenes.push_back(newScene);

	UE_LOG("Created new scene\n");
	return GetActiveScene();
}

void SceneManager::SaveScene(FString sceneName)
{
	UScene* activeScene = GetActiveScene();
	activeScene->NextUUID = UEngineStatics::NextUUID;

	SerializeHelper::SaveAsJson(activeScene, "Scenes/" + sceneName + ".Scene");
}

UScene* SceneManager::LoadScene(FString sceneName)
{
	const FString filePath = "Scenes/" + sceneName + ".Scene";
	UScene* loadedScene = SerializeHelper::LoadFromJson<UScene>(filePath);

	if (loadedScene != nullptr)
	{
		LoadedScenes.push_back(loadedScene);
		SetActiveScene(loadedScene);
		UEngineStatics::NextUUID = loadedScene->NextUUID;
	}

	return loadedScene;
}

void SceneManager::UnloadScene(UScene* targetScene)
{
	//UScene* curScene = GetActiveScene();
	UScene* sceneToUnload = (targetScene != nullptr) ? targetScene : GetActiveScene();
	if (sceneToUnload == nullptr)
	{
		return;
	}
	auto sceneIt = std::find(LoadedScenes.begin(), LoadedScenes.end(), sceneToUnload);
	if (sceneIt == LoadedScenes.end())
	{
		return;
	}
	// loop 내에서의 삽입/삭제로 인한 오류 방지
	auto deleteList = sceneToUnload->SceneComponents;
	for (auto* obj : deleteList)
	{
		RemoveSceneComponent(obj, sceneToUnload);
	}

	delete sceneToUnload;
	LoadedScenes.erase(sceneIt);

	if (ActiveScene == sceneToUnload)
	{
		SetActiveScene(LoadedScenes.empty() ? nullptr : LoadedScenes.back());
	}
}

void SceneManager::RemoveSceneComponent(USceneComponent* toDespawn, UScene* targetScene)
{
	UScene* scene = (targetScene != nullptr) ? targetScene : ActiveScene;
	if (scene == nullptr || toDespawn == nullptr)
	{
		return;
	}

	// Remove render object first
	auto primToDespawn = Cast<UPrimitiveComponent>(toDespawn);
	if (primToDespawn != nullptr)
	{
		FLevel::GetInstance().UnregisterRenderObject(primToDespawn->GetRenderObject());
	}

	for (auto it = scene->SceneComponents.begin(); it != scene->SceneComponents.end(); ++it)
	{
		if (*it == toDespawn)
		{
			delete toDespawn;
			scene->SceneComponents.erase(it);
			break;
		}
	}
}
