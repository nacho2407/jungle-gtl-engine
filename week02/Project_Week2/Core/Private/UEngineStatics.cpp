#include "UEngineStatics.h"



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