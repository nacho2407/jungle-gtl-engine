#pragma once

#include "Engine/Object/Object.h"

struct FTypeInfo;

inline TArray<const FTypeInfo*>& GetClassRegistry()
{
	static TArray<const FTypeInfo*> Registry;
	return Registry;
}

// Define Class 저장소
struct FClassRegistrar
{
	FClassRegistrar(const FTypeInfo* Info)
	{
		GetClassRegistry().push_back(Info);
	}
};