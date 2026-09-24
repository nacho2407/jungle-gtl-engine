#pragma once

#include "Scene.h"
#include "Singleton.h"
#include "EngineCore/EngineTypes.h"//Fstring
#include "EngineCore/Logger.h"
#include "Renderer/Level.h"
#include "Component/PrimitiveComponent.h"
#include "EngineCore/ObjectFactory.h"
#include "EngineCore/EngineStatics.h"
#include "SerializeHelper.h"
#include <filesystem>

class UWorld;

class SceneManager
{
public:
	SceneManager();
	~SceneManager();

public:
	// === Scene Management
	const TArray<UScene*>& GetLoadedScenes() const;
	UScene* GetActiveScene();
	void SetActiveScene(UScene*);
	UScene* GetPermanentScene();
	void SetPermanentScene(UScene*);
	UScene* NewScene();
	void SaveScene(FString sceneName);
	UScene* LoadScene(FString sceneName);
	void UnloadScene(UScene* targetScene, bool bCreateReplacementScene = true);
	// === Scene Management
	
	template <typename T>
	T* AddSceneComponent(UScene* targetScene)
	{
		static_assert(std::is_base_of<USceneComponent, T>::value, "T must derive from USceneComponent");

		if (targetScene == nullptr)
		{
			return nullptr;
		}

		UObject* obj = FObjectFactory::ConstructObject(T::GetClass());
		T* instance = Cast<T>(obj);
		if (instance == nullptr)
		{
			return nullptr;
		}

		instance->Scene = targetScene;

		targetScene->SceneComponents.push_back(instance);
		instance->OnComponentAdded();
		return instance;
	}

	template <typename T>
	T* AddSceneComponent()
	{
		return AddSceneComponent<T>(ActiveScene);
	}

	template <typename T>
	T* AddPermanentSceneComponent()
	{
		return AddSceneComponent<T>(PermanentScene);
	}

	void RemoveSceneComponent(USceneComponent* toDespawn, UScene* targetScene= nullptr);

	UWorld* OwnerWorld = nullptr;
	UScene* ActiveScene = nullptr;
	UScene* PermanentScene = nullptr;
	TArray<UScene*> LoadedScenes;
};
