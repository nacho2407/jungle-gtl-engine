#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

struct FAmbientLightInfo
{
    FVector Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    FVector Color;
    float Intensity;
    FVector Direction;
    float Padding;
};

#define MAX_DIRECTIONAL_LIGHTS 4

struct FGlobalLightConstants
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHTS];
    int32 NumDirectionalLights;
    int32 NumLocalLights;
    FVector2 Padding;
};

struct FLocalLightInfo
{
    FVector Color;
    float Intensity;
    FVector Position;
    float AttenuationRadius;
    FVector Direction;
    float InnerConeAngle;
    float OuterConeAngle;
    float Padding[3];
};

struct FCollectedLights
{
    FGlobalLightConstants GlobalLights;
    TArray<FLocalLightInfo> LocalLights;
};
