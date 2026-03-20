#pragma once

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

struct FMeshVertex
{
    FVector Position;
    FVector Normal;
    FVector2 UV;
};

struct FLineVertex
{
    FVector Position;
    FVector4 Color;
};

struct FFontVertex
{
    FVector Position;
    FVector2 UV;
    FVector4 Color;
};

struct FSpriteVertex
{
    FVector Position;
    FVector2 UV;
    FVector4 Color;
};