#pragma once

#include "Core/Math/Vector.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Color.h"

struct FMeshVertex
{
    FVector Position;
};

struct FLineVertex
{
    FVector Position;
    FColor  Color;
};

struct FFontVertex
{
    FVector  Position;
    FVector2 UV;
    FColor   Color;
};

struct FSpriteVertex
{
    FVector  Position;
    FVector2 UV;
    FColor   Color;
};