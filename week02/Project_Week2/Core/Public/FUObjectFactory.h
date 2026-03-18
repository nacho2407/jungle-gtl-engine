#pragma once
#include "Actor/AActor.h"
#include "FUObjectAllocator.h"
#include "UEngineStatics.h"
#include "UObject.h"
#include "Component/USceneComponent.h"
#include "FUObjectArray.h"

template<typename T>
concept UObjectType = std::is_base_of_v<UObject, T>;

template<typename T>
concept SceneComponentType = std::is_base_of_v<USceneComponent, T>;

template<typename T>
concept AActprType = std::is_base_of_v<AActor, T>;

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
T* NewObject(const FString& InName = "GameObject")
{
	return GUObjectFactory.CreateObject<T>(InName);
}

template<SceneComponentType T>
T* NewObject(const FVector& Location, const FString& InName = "GameObject")
{
	T* object = NewObject<T>(InName);
	object->SetRelativeLocation(Location);
	return object;
}

template<AActprType T>
T* NewObject(const FVector& Location, const FString& InName = "GameObject")
{
	T* object = NewObject<T>(InName);
	object->SetRelativeLocation(Location);
	return object;
}

template<UObjectType T>
inline void* DestroyObject(T* object)
{
	auto TargetIndex = object->InternalIndex;
	GUObjectArray.FreeUObjectIndox(object);
	GUObjectAllocator.FreeUObject(object);
	return nullptr;
}