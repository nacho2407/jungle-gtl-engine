// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

// ELightType는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ELightType : uint32
{
    Ambient = 0,
    Directional,
    Point,
    Spot,
};

// FAmbientLightInfo는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
struct FAmbientLightInfo
{
    FVector Color;
    float   Intensity;
};

// FDirectionalLightInfo는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
struct FDirectionalLightInfo
{
    FVector Color;
    float   Intensity;
    FVector Direction;
    float   Padding;
};

#define MAX_DIRECTIONAL_LIGHTS 4

// FGlobalLightConstants는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
struct FGlobalLightConstants
{
    FAmbientLightInfo     Ambient;
    FDirectionalLightInfo Directional[MAX_DIRECTIONAL_LIGHTS];
    int32                 NumDirectionalLights;
    int32                 NumLocalLights;
    FVector2              Padding;
};

// FLocalLightInfo는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
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

// FLightConstants는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
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
