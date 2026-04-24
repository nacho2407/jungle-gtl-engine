
/*
    CommonTypes.hlsli는 셰이딩 표면 데이터 타입을 정의합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

#ifndef COMMON_TYPES_HLSLI
#define COMMON_TYPES_HLSLI

#include "../Utils/Functions.hlsl"
#include "../Resources/SystemSamplers.hlsl"
#include "../Resources/SystemResources.hlsl"
#include "../Material/StaticMeshMaterialCommon.hlsli"

struct FOpaqueVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD3;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD1;
    float4 gouraud      : TEXCOORD2;
};

struct FOpaqueOutput2
{
    float4 BaseColor : SV_TARGET0;
    float4 Surface1  : SV_TARGET1;
};

struct FOpaqueOutput3
{
    float4 BaseColor : SV_TARGET0;
    float4 Surface1  : SV_TARGET1;
    float4 Surface2  : SV_TARGET2;
};

struct FDecalOutput2
{
    float4 ModifiedBaseColor : SV_TARGET0;
    float4 ModifiedSurface1  : SV_TARGET1;
};

struct FDecalOutput3
{
    float4 ModifiedBaseColor : SV_TARGET0;
    float4 ModifiedSurface1  : SV_TARGET1;
    float4 ModifiedSurface2  : SV_TARGET2;
};

#endif

