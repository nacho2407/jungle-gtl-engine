#ifndef OUTLINE_BUFFER_HLSL
#define OUTLINE_BUFFER_HLSL

// b3: Outline 설정
cbuffer OutlinePostProcessCB : register(b3)
{
    float4 OutlineColor; // 아웃라인 색상 + 알파
    float OutlineThickness; // 샘플링 오프셋 (픽셀 단위, 보통 1.0)
    float3 _Pad;
};

#endif // OUTLINE_BUFFER_HLSL
