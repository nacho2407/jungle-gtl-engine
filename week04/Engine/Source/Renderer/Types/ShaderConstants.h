#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector4.h"

// Non-instanced unlit mesh
struct alignas(16) FMeshUnlitConstants
{
    FMatrix MVP;
    FMatrix World;
    FColor  BaseColor;
};

// Instanced unlit mesh
struct alignas(16) FMeshUnlitInstancedConstants
{
    FMatrix VP;
};

// Reserved. Not used in this project.
struct alignas(16) FMeshLitConstants
{
    // ...
};

struct alignas(16) FLineConstants
{
    FMatrix VP;
    float   CameraPos[3];
    float   MaxDistance;
};

struct alignas(16) FFontConstants
{
    FMatrix VP;
    FColor  TintColor;
};

struct alignas(16) FSpriteConstants
{
    FMatrix VP;
};