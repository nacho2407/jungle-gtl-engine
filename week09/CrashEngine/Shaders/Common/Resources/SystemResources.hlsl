/*
    SystemResources.hlsl는 공용 시스템 텍스처 슬롯 선언을 제공합니다.

    바인딩 컨벤션
    - t10: SceneDepth
    - t11: SceneColor
    - t13: Stencil
    - 이 파일에서 직접 선언하는 슬롯: t10, t11, t13
*/

#ifndef SYSTEM_RESOURCES_HLSL
#define SYSTEM_RESOURCES_HLSL

Texture2D<float>  SceneDepth  : register(t10);
Texture2D<float4> SceneColor  : register(t11);
Texture2D<uint2>  StencilTex  : register(t13);

#endif
