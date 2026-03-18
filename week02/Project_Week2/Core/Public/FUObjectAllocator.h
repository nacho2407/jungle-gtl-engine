#pragma once
#include "CoreTypes.h"
#include "FMemory.h"
#include "UObject.h"

class FUObjectAllocator
{
public:

	void* AllocateUObject(uint32 Size)
	{
		return (UObject*) FMemory::Malloc(Size);
	}

	template<typename T>
	T* AllocateUObject()
	{
		void* mem = AllocateUObject(sizeof(T));
		return new (mem) T();
	}

	template<typename T>
	T* AllocateUObject(const FUObjectInitializer& initializer)
	{
		void* mem = AllocateUObject(sizeof(T));
		T* UObjectPtr = new (mem) T();
		UObjectPtr->Initialize(initializer);
		return UObjectPtr;
	}

	template<typename T>
	void FreeUObject(T* obj)
	{
		size_t Size = obj->GetClass()->ClassSize;
		if (!obj) return;
		obj->~T();
		FMemory::Free(obj, Size);
	}
};

inline FUObjectAllocator GUObjectAllocator;