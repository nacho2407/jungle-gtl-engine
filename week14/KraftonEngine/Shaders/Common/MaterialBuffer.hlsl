#ifndef MATERIAL_BUFFER_HLSL
#define MATERIAL_BUFFER_HLSL

// b4: Material properties
cbuffer MaterialBuffer : register(b4)
{
    uint bIsUVScroll;
    float3 _matPad;
    float4 SectionColor;
}

#endif // MATERIAL_BUFFER_HLSL
