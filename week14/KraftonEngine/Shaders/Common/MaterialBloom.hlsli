#ifndef MATERIAL_BLOOM_HLSLI
#define MATERIAL_BLOOM_HLSLI

cbuffer MaterialBloomCB : register(b8)
{
    float4 MaterialEmissiveColor;
    float MaterialEmissiveIntensity;
    float MaterialBloomEnabled;
    float2 MaterialBloomPadding;
};

float3 GetMaterialEmissive(float3 sourceColor)
{
    return sourceColor * MaterialEmissiveColor.rgb * max(MaterialEmissiveIntensity, 0.0f);
}

float3 ApplyMaterialEmissive(float3 litColor, float3 sourceColor)
{
    float3 result = litColor + GetMaterialEmissive(sourceColor);
    return MaterialBloomEnabled >= 0.5f ? result : saturate(result);
}

float3 ApplyMaterialEmissive(float3 color)
{
    return ApplyMaterialEmissive(color, color);
}

#endif
