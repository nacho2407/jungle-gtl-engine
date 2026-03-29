#pragma once
#include "Core/CoreTypes.h"

class MemoryStats
{
public:
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

	static uint32 GetTotalAllocationBytes() { return TotalAllocationBytes; }
	static uint32 GetTotalAllocationCount() { return TotalAllocationCount; }
	static uint32 GetPixelShaderMemory() { return PixelShaderMemory; }
	static uint32 GetVertexShaderMemory() { return VertexShaderMemory; }

private:
	static uint32 TotalAllocationBytes;
	static uint32 TotalAllocationCount;
	static uint32 PixelShaderMemory;
	static uint32 VertexShaderMemory;
};
