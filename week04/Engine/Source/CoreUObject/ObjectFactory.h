#pragma once

#include "Core/CoreMinimal.h"
#include <functional>

class UObject;

struct FObjectFactory
{
public:
	static UObject* ConstructObject(const void* Id);

	static void RegisterObjectType(const void* Id, std::function<UObject*()> FactoryFunction);

private:
	static TMap<const void*, std::function<UObject*()>>& GetRegistry()
	{
		static TMap<const void*, std::function<UObject*()>> Registry;
		return Registry;
	}
};
