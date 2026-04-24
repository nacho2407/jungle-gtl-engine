// Defines CPU-side light submission types shared by light proxies and collection.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

// ELightType identifies the logical light kind used during render submission.
enum class ELightType : uint32
{
    Ambient = 0,
    Directional,
    Point,
    Spot,
};

// FLightProxyInfo stores the CPU-side light data a proxy submits to the renderer.
struct FLightProxyInfo
{
    FVector  Position;
    float    Intensity;
    FVector  Direction;
    float    AttenuationRadius;
    FVector4 LightColor;
    float    InnerConeAngle;
    float    OuterConeAngle;
    uint32   LightType;
    float    Padding;
};
