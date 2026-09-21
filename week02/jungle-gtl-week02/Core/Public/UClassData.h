#pragma once

#include "CoreTypes.h"

class UClassData
{
public:
	FString ClassName;
	int32 ClassSize;
	UClassData* SuperClass = nullptr;
};
