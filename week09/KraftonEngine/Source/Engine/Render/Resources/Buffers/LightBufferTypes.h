#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

enum class ELightType : uint32
{
    Ambient = 0,
    Directional,
    Point,
    Spot,
};

struct FAmbientLightInfo
{
    FVector Color;
    float   Intensity;
};

struct FDirectionalLightInfo
{
    FVector Color;
    float   Intensity;
    FVector Direction;
    float   Padding;
};

#define MAX_DIRECTIONAL_LIGHTS 4

struct FGlobalLightConstants
{
    FAmbientLightInfo     Ambient;
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHTS];
    int32                 NumDirectionalLights;
    int32                 NumLocalLights;
    FVector2              Padding;
};

struct FLocalLightInfo
{
    FVector Color;
    float   Intensity;
    FVector Position;
    float   AttenuationRadius;
    FVector Direction;
    float   InnerConeAngle;
    float   OuterConeAngle;
    float   Padding[3];
};

struct FLightConstants
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
