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

	template <typename T>
	T* AddSceneComponent();
	void RemoveSceneComponent(USceneComponent* toDespawn);

private:
	static UScene* activeScene;
};

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}