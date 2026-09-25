#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Color.h"

struct alignas(16) FMeshInstanceData
{
    FMatrix  World;
    FColor Color = FColor::White();
};