    #pragma once

    #include "Core/CoreTypes.h"
    #include "Math/Vector.h"

    /*
        라이트 종류와 GPU 업로드용 라이트 데이터 구조를 정의하는 헤더입니다.
        씬 수집 단계와 라이팅 패스가 공통으로 사용합니다.
    */
    enum class ELightType
{
    Directional,
    Point,
    Spot,
    Ambient,
};

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
