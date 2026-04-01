#pragma once

#include "Core/CoreMinimal.h"

struct FPlatformMemoryStats
{
	uint64 TotalPhysical = 0;
	uint64 AvailablePhysical = 0;
	uint64 UsedPhysical = 0;
	uint64 TotalVirtual = 0;
	uint64 AvailableVirtual = 0;
	uint64 UsedVirtual = 0;
};
