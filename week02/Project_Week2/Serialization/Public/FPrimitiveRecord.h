#pragma once

#include "CoreTypes.h"
#include "Math/FVector.h"

struct FPrimitiveRecord
{
    FString Type;
    FVector Location;
    FVector Rotation;
    FVector Scale;
};
