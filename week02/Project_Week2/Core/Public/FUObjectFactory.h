#pragma once
#include "FUObjectAllocator.h"
#include "UEngineStatics.h"
#include "UObject.h"
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