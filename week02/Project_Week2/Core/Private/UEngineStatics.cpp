#include "UEngineStatics.h"

uint32 UEngineStatics::NextUUID = 1;
uint32 UEngineStatics::TotalAllocationBytes = 0;
uint32 UEngineStatics::TotalAllocationCount = 0;

uint32 UEngineStatics::GenUUID()
{
    return NextUUID++;
}

void UEngineStatics::Initialize()
{
    NextUUID = 1;
    TotalAllocationBytes = 0;
    TotalAllocationCount = 0;
}