#pragma once
#include "UEngineStatics.h"
#include "UObject.h"

class FUObjectFactory
{
public:
	template<typename T = UObject>
	static T* ConstructObject(FString InName)
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must inherit from UObject");
		T* NewObject = new T();

		NewObject->Init(UEngineStatics::GenUUID(), InName);
		return NewObject;
	}
}; 