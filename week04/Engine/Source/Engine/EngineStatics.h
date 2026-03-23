#pragma once

#include "Core/EngineAPI.h"
#include "Core/HAL/PlatformTypes.h"

class ENGINE_API UEngineStatics
{
public:
	static uint32 GenUUID();

	static uint32 NextUUID;
	static uint32 TotalAllocatedBytes;
	static uint32 TotalAllocationCount;
};
