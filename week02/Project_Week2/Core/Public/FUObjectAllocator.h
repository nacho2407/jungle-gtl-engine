#pragma once
#include "CoreTypes.h"
#include "FMemory.h"
#include "UObject.h"

class FUObjectAllocator
{
public:

	void* AllocateUObject(uint32 Size)
	{
		return (UObjectBase*) FMemory::Malloc(Size);
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
		return new (mem) T(initializer);
	}

	template<typename T>
	void FreeUObject(T* obj)
	{
		size_t Size = sizeof(T);
		if (!obj) return;
		obj->~T();
		FMemory::Free(obj, static_cast<size_t>(Size));
	}
};

inline FUObjectAllocator GUObjectAllocator;