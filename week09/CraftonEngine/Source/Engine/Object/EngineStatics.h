#pragma once
#include "Core/CoreTypes.h"

class EngineStatics
{
private:
	static uint32 NextUUID;

public:

	static uint32 TotalAllocationBytes;
	static uint32 TotalAllocationCount;
	static uint32 PixelShaderMemory;
	static uint32 VertexShaderMemory;

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

	static void AddPixelShaderMemory(uint32 Size) { PixelShaderMemory += Size; }
	static void SubPixelShaderMemory(uint32 Size) { PixelShaderMemory -= Size; }

	static void AddVertexShaderMemory(uint32 Size) { VertexShaderMemory += Size; }
	static void SubVertexShaderMemory(uint32 Size) { VertexShaderMemory -= Size; }

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

	inline static uint32 GetPixelShaderMemory() { return PixelShaderMemory; }
	inline static uint32 GetVertexShaderMemory() { return VertexShaderMemory; }

	static uint32 GenUUID()
	{
		return NextUUID++;
	}
};
