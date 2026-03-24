#include "SceneManager.h"

#include "Component/BillboardTextComponent.h"
#include "Component/CameraComponent.h"

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

void SceneManager::AttachDefaultBillboardText(UScene* targetScene, USceneComponent* targetComponent)
{
	if (targetScene == nullptr || targetComponent == nullptr || targetScene == PermanentScene)
	{
		return;
	}

	if (Cast<UBillboardTextComponent>(targetComponent) != nullptr)
	{
		return;
	}

	if (Cast<UCameraComponent>(targetComponent) != nullptr)
	{
		return;
	}

	// 에디터에서 배치한 실제 오브젝트들만 자동 라벨 대상에 포함합니다.
	UBillboardTextComponent* BillboardText = AddSceneComponent<UBillboardTextComponent>(targetScene);
	if (BillboardText != nullptr)
	{
		BillboardText->AttachTo(targetComponent);
	}
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
		loadedScene->Name = FName(sceneName);
		LoadedScenes.push_back(loadedScene);
		SetActiveScene(loadedScene);
		UEngineStatics::NextUUID = loadedScene->NextUUID;

		TArray<USceneComponent*> Components = loadedScene->SceneComponents;
		for (USceneComponent* Component : Components)
		{
			AttachDefaultBillboardText(loadedScene, Component);
		}
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

	auto deleteList = sceneToUnload->SceneComponents;
	for (auto* obj : deleteList)
	{
		RemoveSceneComponent(obj, sceneToUnload);
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

	if (Cast<UBillboardTextComponent>(toDespawn) == nullptr)
	{
		TArray<USceneComponent*> LabelsToRemove;
		for (USceneComponent* Component : scene->SceneComponents)
		{
			UBillboardTextComponent* BillboardText = Cast<UBillboardTextComponent>(Component);
			if (BillboardText != nullptr && BillboardText->GetAttachedComponent() == toDespawn)
			{
				LabelsToRemove.push_back(BillboardText);
			}
		}

		for (USceneComponent* Label : LabelsToRemove)
		{
			RemoveSceneComponent(Label, scene);
		}
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
			break;
		}
	}
}
