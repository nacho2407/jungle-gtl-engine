#pragma once

#include "Math/FVector.h"
#include "Math/FVector4.h"

struct FVertexSimple
{
    FVector Position; // 12 bytes
    FVector4 Color; // 16 bytes

    FVertexSimple() = default;

    FVertexSimple(const FVector& InPosition, const FVector4& InColor)
        : Position(InPosition), Color(InColor) {}

    FVertexSimple(float x, float y, float z, float r, float g, float b, float a)
        : Position(x, y, z), Color(r, g, b, a) {}
};

enum class EGizmoAxis
{
    None,
    X,
    Y,
    Z
};