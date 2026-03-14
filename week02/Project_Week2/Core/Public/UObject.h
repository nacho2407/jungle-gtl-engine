#pragma once
#include "Core.h"

class UObject
{
public:
	void* operator new(size_t size)
	{
		return nullptr;
	}
	void operator delete(void* ptr) noexcept
	{
	}
	uint32 UUID;
	uint32 InternalIndex; // Index of GUObjectArray
	virtual void Init() = 0;
	virtual void Start() = 0;
	//const type_info& GetType();

	//template<typename T>
	//T* UObject::CreateDefaultSubobject(FString FName)
	//{
	//	return nullptr;
	//}
};