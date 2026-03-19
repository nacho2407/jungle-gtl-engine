#include "EngineStatics.h"

UEngineStatics::uint32 GenUUID()
{
	return NextUUID++;
}

uint32 UEngineStatics::NextUUID = 0;
uint32 UEngineStatics::TotalAllocatedBytes = 0;
uint32 UEngineStatics::TotalAllocateCount = 0;