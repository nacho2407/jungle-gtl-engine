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
		UObject* instance = FObjectFactory::ConstructObject(T::GetClass());
		T* ret = Cast<T>(instance);

		activeScene->Primitives.push_back(ret);

		activeScene->NextUUID += 1;
		return ret;
	}

	void RemoveSceneComponent(USceneComponent* toDespawn);

private:
	static UScene* activeScene;
};

UWorld& GetWorld();