#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"

struct alignas(16) FMeshInstanceData
{
    FMatrix  World;
    FVector4 Color = FVector4(1, 1, 1, 1);
};