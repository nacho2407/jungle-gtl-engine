#include "EngineStatics.h"

uint32 EngineStatics::NextUUID = 1;

uint32 EngineStatics::TotalAllocationBytes = 0;
uint32 EngineStatics::TotalAllocationCount = 0;

void* operator new(size_t Size)
{
	void* Ptr = std::malloc(Size);
	if (Ptr)
	{
		EngineStatics::OnAllocated(static_cast<uint32>(Size));
	}
	return Ptr;
}

void operator delete(void* Ptr, size_t Size)
{
	if (Ptr)
	{
		EngineStatics::OnDeallocated(static_cast<uint32>(Size));
		std::free(Ptr);
		;
	}
}