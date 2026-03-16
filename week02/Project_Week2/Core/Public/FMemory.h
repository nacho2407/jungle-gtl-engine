#pragma once
#include "CoreTypes.h"

class FMemory
{
private:
	static uint32 TotalAllocatedBytes;

public:
	static void* Malloc(size_t size)
	{
		TotalAllocatedBytes += static_cast<uint32>(size);
		return ::operator new(size);
	}

	static void Free(void* ptr, size_t size)
	{
		TotalAllocatedBytes -= static_cast<uint32>(size);
		delete ptr;
	}

	static uint32 GetTotalAllocatedMemory()
	{
		return TotalAllocatedBytes;
	}

	static void* Memzero(void* Dest, size_t Size)
	{
		return memset(Dest, 0, Size);
	}
};

inline uint32 FMemory::TotalAllocatedBytes = 0;