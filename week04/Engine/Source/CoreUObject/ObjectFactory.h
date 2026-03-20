#pragma once

#include "../Core/CoreMinimal.h"
#include "Core/Containers/Map.h"
#include <functional>

class UObject;

struct FObjectFactory
{
public:
	static UObject* ConstructObject(const void* id);

	static void RegisterObjectType(const void* id, std::function<UObject*()> func);

private:
	static TMap<const void*, std::function<UObject*()>>& GetRegistry()
	{
		static TMap<const void*, std::function<UObject*()>> registry;
		return registry;
	}
};

