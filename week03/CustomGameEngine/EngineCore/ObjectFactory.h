#pragma once

#include "EngineCore/EngineTypes.h"
#include <functional>

class UObject;

struct FObjectFactory
{
public:
	static UObject* ConstructObject(const void* id);

	static void RegisterObjectType(const void* id, std::function<UObject* ()> func);

private:
	static TUMap<const void*, std::function<UObject* ()>>& GetRegistry()
	{
		static TUMap<const void*, std::function<UObject* ()>> registry;
		return registry;
	}
};

