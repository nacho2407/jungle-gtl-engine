#include "SceneManager.h"

class UEngineStatics;

namespace
{
	static void ShrinkSceneStorageIfEmpty(UScene* scene)
	{
		if (scene == nullptr)
		{
			return;
		}

		if (scene->SceneComponents.empty())
		{
			scene->SceneComponents.shrink_to_fit();
		}

		if (scene->Level.RenderObjects.empty())
		{
			scene->Level.RenderObjects.shrink_to_fit();
		}
	}
}

SceneManager::SceneManager()
{
	PermanentScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
}

SceneManager::~SceneManager()
{
	if (PermanentScene != nullptr)
	{
		while (!PermanentScene->SceneComponents.empty())
		{
			RemoveSceneComponent(PermanentScene->SceneComponents.back(), PermanentScene);
		}

		delete PermanentScene;
		PermanentScene = nullptr;
	}
}

const TArray<UScene*>& SceneManager::GetLoadedScenes() const
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
	SetActiveScene(newScene);
	LoadedScenes.push_back(newScene);

	UE_LOG("Created new scene\n");
	return GetActiveScene();
}

void SceneManager::SaveScene(FString sceneName)
{
	UScene* activeScene = GetActiveScene();
	activeScene->NextUUID = UEngineStatics::NextUUID;

	const FString filePath = "Scenes/";
	const std::filesystem::path SceneDirectory(filePath.c_str());

	if (!std::filesystem::exists(SceneDirectory))
	{
		std::filesystem::create_directories(SceneDirectory);
	}

	SerializeHelper::SaveAsJson(activeScene, "Scenes/" + sceneName + ".Scene");
}

UScene* SceneManager::LoadScene(FString sceneName)
{
	const FString filePath = "Scenes/" + sceneName + ".Scene";
	UScene* loadedScene = SerializeHelper::LoadFromJson<UScene>(filePath);

	if (loadedScene != nullptr)
	{
		loadedScene->Name = FName(sceneName);
		LoadedScenes.push_back(loadedScene);
		SetActiveScene(loadedScene);
		UEngineStatics::NextUUID = loadedScene->NextUUID;
	}

	return loadedScene;
}

void SceneManager::UnloadScene(UScene* targetScene, bool bCreateReplacementScene)
{
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

	if (ActiveScene == sceneToUnload)
	{
		UScene* nextActiveScene = nullptr;
		if (LoadedScenes.size() > 1)
		{
			const int32 sceneIndex = static_cast<int32>(sceneIt - LoadedScenes.begin());
			const int32 fallbackIndex = (sceneIndex > 0) ? sceneIndex - 1 : 1;
			nextActiveScene = LoadedScenes[fallbackIndex];
		}
		SetActiveScene(nextActiveScene);
	}

	while (!sceneToUnload->SceneComponents.empty())
	{
		RemoveSceneComponent(sceneToUnload->SceneComponents.back(), sceneToUnload);
	}

	delete sceneToUnload;
	LoadedScenes.erase(sceneIt);

	if (bCreateReplacementScene && LoadedScenes.empty())
	{
		UE_LOG("No scenes remained after deletion.\n");
		NewScene();
	}
}

void SceneManager::RemoveSceneComponent(USceneComponent* toDespawn, UScene* targetScene)
{
	UScene* scene = (targetScene != nullptr) ? targetScene : ActiveScene;
	if (scene == nullptr || toDespawn == nullptr)
	{
		return;
	}

	auto primToDespawn = Cast<UPrimitiveComponent>(toDespawn);
	if (primToDespawn != nullptr)
	{
		scene->Level.UnregisterRenderObject(primToDespawn->GetRenderObject());
	}

	for (auto it = scene->SceneComponents.begin(); it != scene->SceneComponents.end(); ++it)
	{
		if (*it == toDespawn)
		{
			delete toDespawn;
			scene->SceneComponents.erase(it);
			ShrinkSceneStorageIfEmpty(scene);
			break;
		}
	}
}
