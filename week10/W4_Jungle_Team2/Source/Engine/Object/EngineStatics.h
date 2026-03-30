#pragma once
#include "Core/CoreMinimal.h"

class EngineStatics
{
private:
	static uint32 NextUUID;

public:

	static uint32 TotalAllocationBytes;
	static uint32 TotalAllocationCount;

	static void OnAllocated(uint32 Size)
	{
		TotalAllocationBytes += Size;
		TotalAllocationCount++;
	}

	static void OnDeallocated(uint32 Size)
	{
		TotalAllocationBytes -= Size;
		TotalAllocationCount--;
	}

	// Reset UUID generation to a specific value
	static void ResetUUIDGeneration(int Value) { NextUUID = Value; }

	inline static uint32 GetTotalAllocationBytes()
	{
		return TotalAllocationBytes;
	}

	inline static uint32 GetTotalAllocationCount()
	{
		return TotalAllocationCount;
	}

	static uint32 GenUUID()
	{
		return NextUUID++;
	}
};