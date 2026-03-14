#pragma once

#include "Scene.h"
#include "ISingleton.h"

class UWorld : public UObject, public ISingleton<UWorld>
{
	friend class ISingleton<UWorld>;
private:
	UWorld();

public:
	~UWorld();

	UScene* GetActiveScene();
	void SetActiveScene(UScene*);

	template <typename T>
	T* AddSceneComponent()
	{
		// TODO: 액터 개념 도입되면 USceneComponent 대신 UActor로 교체
		static_assert(std::is_base_of<USceneComponent, T>::value, "T must derive from USceneComponent");

		int uuid = activeScene->NextUUID;
		// TODO: 오브젝트 생성을 FObjectFactory 기반으로 전환
		// T* instance = FObjectFactory::ConstructObject();
		T* instance = new T();

		activeScene->Primitives.push_back(static_cast<USceneComponent*>(instance));

		activeScene->NextUUID += 1;
		return instance;
	}

	void RemoveSceneComponent(USceneComponent* toDespawn);

private:
	static UScene* activeScene;
};

UWorld& GetWorld();