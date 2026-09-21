#pragma once
#include "CoreTypes.h"

struct FUObjectInitializer
{
	uint32 UUID;
	FString Name;

	FUObjectInitializer() : UUID(0), Name("DefaultObject") {}
};

