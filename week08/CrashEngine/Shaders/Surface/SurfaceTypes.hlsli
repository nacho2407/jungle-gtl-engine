#ifndef SURFACE_TYPES_HLSLI
#define SURFACE_TYPES_HLSLI

struct FSurfaceData
{
    float3 BaseColor;
    float  Opacity;

    float3 WorldNormal;
    float  Roughness;

    float  Metallic;
    float  Specular;
    float  AmbientOcclusion;
    float  Padding;

    float4 Gouraud;
};

#endif
