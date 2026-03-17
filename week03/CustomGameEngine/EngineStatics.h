#pragma once

#include "EngineTypes.h"

class UEngineStatics
{
public:
	static uint32 GenUUID()
	{
		return NextUUID++;
	}
	
	static uint32 NextUUID;

	static uint32 TotalAllocatedBytes;
	static uint32 TotalAllocationCount;
};

