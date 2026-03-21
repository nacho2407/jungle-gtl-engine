#pragma once

#include "Math/Matrix.h"
#include "Math/Vector4.h"

struct alignas(16) FMeshUnlitConstants
{
    FMatrix  MVP;
    FVector4 BaseColor;
};

struct alignas(16) FMeshLitConstants
{
    FMatrix  W;
    FMatrix  MVP;
    FVector4 BaseColor;
    FVector4 LightDirection;
};

struct alignas(16) FLineConstants
{
    FMatrix VP;
};

struct alignas(16) FFontConstants
{
    FMatrix  P;
    FVector4 TintColor;
};

struct alignas(16) FSpriteConstants
{
    FMatrix VP;
};