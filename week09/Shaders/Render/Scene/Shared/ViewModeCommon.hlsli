/*
    ViewModeCommon.hlsli는 scene view mode 셰이더에서 공유하는 helper를 제공합니다.

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

#ifndef VIEW_MODE_COMMON_HLSLI
#define VIEW_MODE_COMMON_HLSLI

#include "FullscreenPassTypes.hlsli"
#include "../../../Surface/GBufferPacking.hlsli"
#include "../Lighting/DirectLighting.hlsli"

float4 ResolveViewModeSurfaceValue(Texture2D BaseTexture, Texture2D ModifiedTexture, float2 UV)
{
    float4 BaseValue = BaseTexture.Sample(LinearClampSampler, UV);
    float4 ModifiedValue = ModifiedTexture.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(BaseValue, ModifiedValue);
}

#endif
