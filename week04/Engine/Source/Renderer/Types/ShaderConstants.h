#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"

// Non-instanced unlit mesh
struct alignas(16) FMeshUnlitConstants
{
    FMatrix  MVP;
    FVector4 BaseColor;
};

// Instanced unlit mesh
struct alignas(16) FMeshUnlitInstancedConstants
{
    FMatrix VP;
};

// Reserved. Not used in this project.
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