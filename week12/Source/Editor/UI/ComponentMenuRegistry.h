#pragma once

#include "Core/Containers/Array.h"

class UClass;

class FComponentMenuRegistry
{
public:
	static void CollectSpawnableComponentClasses(TArray<UClass*>& OutClasses);
	static UClass* DrawSpawnableComponentClassMenu();
	static const char* GetClassMenuCategory(const UClass* Class);
};
