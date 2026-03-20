#include "EngineStatics.h"

uint32 UEngineStatics::GenUUID()
{
	return NextUUID++;
}

uint32 UEngineStatics::NextUUID = 0;
uint32 UEngineStatics::TotalAllocatedBytes = 0;
uint32 UEngineStatics::TotalAllocationCount = 0;