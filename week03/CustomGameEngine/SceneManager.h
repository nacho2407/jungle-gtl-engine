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

class UWorld;

class SceneManager
{
public:
	SceneManager();
	~SceneManager();

public:
	// === Scene Management
	UScene* GetActiveScene();
	void SetActiveScene(UScene*);
	UScene* GetPermanentScene();
	void SetPermanentScene(UScene*);
	UScene* NewScene();
	void SaveScene(FString sceneName);
	UScene* LoadScene(FString sceneName);
	void UnloadScene(UScene targetScene);
	// === Scene Management
	
	template <typename T>
	T* AddSceneComponent(UScene* targetScene)
	{
		static_assert(std::is_base_of<USceneComponent, T>::value, "T must derive from USceneComponent");

		UObject* obj = FObjectFactory::ConstructObject(T::GetClass());
		T* instance = Cast<T>(obj);

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


private:
	UWorld* OwnerWorld = nullptr;
	UScene* ActiveScene = nullptr;
	UScene* PermanentScene = nullptr;
	TArray<UScene*> LoadedScene;
};
