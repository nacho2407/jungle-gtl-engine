#pragma once

#include "Singleton.h"
#include "Logger.h"		// TEST
#include "Scene.h"
#include "Renderer/Level.h"

class FLevel;

class UWorld : public UObject, public ISingleton<UWorld>
{
	friend class ISingleton<UWorld>;
private:
	UWorld();

public:
	~UWorld();

	// === Scene Management
	UScene* GetActiveScene();
	void SetActiveScene(UScene*);
	UScene* NewScene();
	void SaveScene(FString sceneName);
	UScene* LoadScene(FString sceneName);
	void UnloadScene();
	// === Scene Management

public:
	template <typename T>
	T* AddSceneComponent(UScene* targetScene)
	{
		// TODO: 액터 개념 도입되면 USceneComponent 대신 UActor로 교체
		static_assert(std::is_base_of<USceneComponent, T>::value, "T must derive from USceneComponent");

		UObject* obj = FObjectFactory::ConstructObject(T::GetClass());
		T* instance = Cast<T>(obj);

		targetScene->SceneComponents.push_back(instance);

		auto primitive = Cast<UPrimitiveComponent>(instance);
		if (primitive != nullptr)
		{
			auto renderObj = primitive->CreateRenderObject();
			if(renderObj != nullptr)
				FLevel::GetInstance().PushRenderObject(renderObj);
		}
		UE_LOG("Added new scene component with uuid = %d\n", obj->UUID);
		return instance;
	}

	template <typename T>
	T* AddSceneComponent()
	{
		return AddSceneComponent<T>(activeScene);
	}

	template <typename T>
	T* AddPermanentSceneComponent()
	{
		return AddSceneComponent<T>(permanentScene);
	}

	void RemoveSceneComponent(USceneComponent* toDespawn);

	void Update(float deltaTime);
	inline void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	void OnBeforeRender() const;

private:
	static UScene* activeScene;
	static UScene* permanentScene;
	URenderer* Renderer;
};

UWorld& GetWorld();