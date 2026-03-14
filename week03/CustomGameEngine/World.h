#pragma once

#include "Scene.h"
#include "ISingleton.h"
#include "URender.h"
#include "Logger.h"		// TEST

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

		UObject* obj = FObjectFactory::ConstructObject(T::GetClass());
		T* instance = Cast<T>(obj);

		activeScene->SceneComponents.push_back(instance);

		auto primitive = Cast<UPrimitiveComponent>(instance);
		if (primitive != nullptr)
		{
			activeScene->PrimitiveContainer.push_back(primitive);
		}
		UE_LOG("Added new scene component with uuid = %d\n", obj->UUID);
		return instance;
	}

	void RemoveSceneComponent(USceneComponent* toDespawn);
	inline void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	void Render() const;

private:
	static UScene* activeScene;
	URenderer* Renderer;
};

UWorld& GetWorld();