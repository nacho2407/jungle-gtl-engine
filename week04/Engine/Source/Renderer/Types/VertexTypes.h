#pragma once

#include "Core/Math/Vector.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"

struct FMeshVertex
{
    FVector Position;
};

struct FLineVertex
{
    FVector  Position;
    FVector4 Color;
};

struct FFontVertex
{
    FVector  Position;
    FVector2 UV;
    FVector4 Color;
};

struct FSpriteVertex
{
    FVector  Position;
    FVector2 UV;
    FVector4 Color;
};