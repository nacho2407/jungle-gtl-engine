#pragma once

#include "CoreTypes.h"
#include "Math/FVector.h"

struct FPrimitiveRecord
{
    uint32 SaveID;
    FString Type;
    FVector Location;
    FVector Rotation;
    FVector Scale;
};

struct FWorldSaveData
{
    uint32 Version = 1;
    uint32 NextUUID;
    TArray<FPrimitiveRecord> Primitives;
};
