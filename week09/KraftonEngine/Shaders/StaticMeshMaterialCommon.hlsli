#ifndef STATIC_MESH_MATERIAL_COMMON_HLSLI
#define STATIC_MESH_MATERIAL_COMMON_HLSLI

cbuffer StaticMeshMaterialBuffer : register(b2)
{
    float4 SectionColor;
    float4 MaterialParam;
    uint HasBaseTexture;
    uint HasNormalTexture;
    float2 StaticMeshMaterialPadding;
}

float4 GetStaticMeshSectionColorOrWhite()
{
    float Magnitude = abs(SectionColor.x) + abs(SectionColor.y) + abs(SectionColor.z) + abs(SectionColor.w);
    return (Magnitude < 0.0001f) ? float4(1.0f, 1.0f, 1.0f, 1.0f) : SectionColor;
}

float4 SampleStaticMeshBaseColor(Texture2D TextureRef, float2 UV)
{
    if (HasBaseTexture == 0)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return TextureRef.Sample(LinearWrapSampler, UV);
}

bool StaticMeshHasNormalTexture()
{
    return HasNormalTexture != 0;
}

#endif
