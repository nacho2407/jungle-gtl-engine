#pragma once
#include "Actor/AActor.h"
#include "FUObjectAllocator.h"
#include "UEngineStatics.h"
#include "UObject.h"
#include "Component/USceneComponent.h"

template<typename T>
concept UObjectType = std::is_base_of_v<UObject, T>;

template<typename T>
concept SceneComponentType = std::is_base_of_v<USceneComponent, T>;

class FUObjectFactory
{
public:
	template<typename T>
	T* CreateObject(const FString& InName)
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must inherit from UObject");

		FUObjectInitializer initializer;

		initializer.UUID = UEngineStatics::GenUUID();
		initializer.Name = InName;

		T* object = GUObjectAllocator.AllocateUObject<T>(initializer);

		int32 index = GUObjectArray.GetAvailableIndex();
		int32 SerialNumber = GUObjectArray.AllocateSerialNumber(index);
		GUObjectArray.AllocatdUObjectIndex(object, SerialNumber);

		return object;
	}
};

inline FUObjectFactory GUObjectFactory;

template<UObjectType T>
T* NewObject(const FString& InName)
{
	return GUObjectFactory.CreateObject<T>(InName);
}

template<SceneComponentType T>
T* NewObject(const FString& InName, const FVector& Location)
{
	T* object = NewObject<T>(InName);
	object->SetLocation(Location);
	return object;
}

inline void* Destroy(UObject* object) 
{
	auto TargetIndex = object->InternalIndex;
	GUObjectArray.FreeUObjectIndox(object);
	GUObjectAllocator.FreeUObject(object);
	return nullptr;
}