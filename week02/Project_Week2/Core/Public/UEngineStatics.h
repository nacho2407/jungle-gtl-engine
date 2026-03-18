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

inline uint32 UEngineStatics::NextUUID = 1;
inline uint32 UEngineStatics::TotalAllocationBytes = 0;
inline uint32 UEngineStatics::TotalAllocationCount = 0;