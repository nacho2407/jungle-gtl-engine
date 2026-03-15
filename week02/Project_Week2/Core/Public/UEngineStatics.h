#pragma once
#include "CoreTypes.h"

class UEngineStatics
{
public:

    static uint32 GenUUID();

    static void Initialize();

    static uint32 NextUUID;
public:

    static uint32 TotalAllocationBytes;

    static uint32 TotalAllocationCount;
};