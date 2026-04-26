#ifndef GBUFFER_PACKING_HLSLI
#define GBUFFER_PACKING_HLSLI

#include "SurfaceTypes.hlsli"
#include "SurfacePacking.hlsli"
#include "../Render/Scene/Shared/OpaquePassTypes.hlsli"

FGBufferOutput2 EncodeGBuffer_Gouraud(FSurfaceData Surface)
{
    FGBufferOutput2 Output;
    Output.BaseColor = EncodeBaseColor(float4(Surface.BaseColor, Surface.Opacity));
    Output.Surface1  = Surface.Gouraud;
    return Output;
}

FGBufferOutput2 EncodeGBuffer_Lambert(FSurfaceData Surface)
{
    FGBufferOutput2 Output;
    Output.BaseColor = EncodeBaseColor(float4(Surface.BaseColor, Surface.Opacity));
    Output.Surface1  = EncodeNormal(Surface.WorldNormal);
    return Output;
}

FGBufferOutput3 EncodeGBuffer_BlinnPhong(FSurfaceData Surface)
{
    FGBufferOutput3 Output;
    Output.BaseColor = EncodeBaseColor(float4(Surface.BaseColor, Surface.Opacity));
    Output.Surface1  = EncodeNormal(Surface.WorldNormal);
    Output.Surface2  = EncodeMaterialParam(float4(Surface.Roughness, Surface.Specular, Surface.Metallic, Surface.AmbientOcclusion));
    return Output;
}

FSurfaceData DecodeGBuffer(float4 BaseColor, float4 Surface1, float4 Surface2)
{
    FSurfaceData Surface = (FSurfaceData)0;
    float4 MaterialParam = DecodeMaterialParam(Surface2);
    Surface.BaseColor        = BaseColor.rgb;
    Surface.Opacity          = BaseColor.a;
    Surface.WorldNormal      = DecodeNormal(Surface1);
    Surface.Roughness        = MaterialParam.x;
    Surface.Specular         = MaterialParam.y;
    Surface.Metallic         = MaterialParam.z;
    Surface.AmbientOcclusion = MaterialParam.w;
    Surface.Gouraud          = Surface1;
    return Surface;
}

#endif
