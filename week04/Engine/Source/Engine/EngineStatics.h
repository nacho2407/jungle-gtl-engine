#pragma once

#include "Core/HAL/PlatformTypes.h"

class UEngineStatics
{
public:
	static uint32 GenUUID();

	static uint32 NextUUID;
	static uint32 TotalAllocatedBytes;
	static uint32 TotalAllocationCount;
};
