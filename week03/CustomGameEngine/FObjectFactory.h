#pragma once
#include "UEngineTypes.h"
#include <functional>

class UObject;

struct FObjectFactory
{
public:
	static UObject* ConstructObject(const void* id);

	static void RegisterClass(const void* id, std::function<UObject*()> func);

private:
	static TMap<const void*, std::function<UObject*()>>& GetRegistry()
	{
		static TMap<const void*, std::function<UObject*()>> registry;
		return registry;
	}
};

