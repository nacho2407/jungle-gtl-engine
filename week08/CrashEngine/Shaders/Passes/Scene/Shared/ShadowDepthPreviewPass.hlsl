cbuffer ShadowDebugPreviewCB : register(b2)
{
    float4x4 InvViewProj;
    uint ShadowDepthPreviewMode;
    uint ShadowFilterMethod;
    float ShadowESMExponent;
    float ShadowDebugPreviewPadding;
    float4 AtlasUVScaleOffset;
};

Texture2D<float4> ShadowDebugTexture : register(t0);

struct VSOut
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

VSOut VS(uint VertexID : SV_VertexID)
{
    VSOut Out;
    Out.UV = float2((VertexID << 1) & 2, VertexID & 2);
    Out.Position = float4(Out.UV * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return Out;
}

float3 ReconstructWorld(float2 UV, float Depth)
{
    float2 NdcXY = float2(UV.x * 2.0 - 1.0, (1.0 - UV.y) * 2.0 - 1.0);
    float4 World = mul(float4(NdcXY, Depth, 1.0), InvViewProj);
    return World.xyz / max(World.w, 1e-6);
}

float4 PS(VSOut Input) : SV_Target
{
    uint Width, Height;
    ShadowDebugTexture.GetDimensions(Width, Height);
    const float2 AtlasUV = float2(
        AtlasUVScaleOffset.z + Input.UV.x * AtlasUVScaleOffset.x,
        AtlasUVScaleOffset.w + Input.UV.y * AtlasUVScaleOffset.y);
    const uint2 PixelCoord = uint2(
        min((uint)(AtlasUV.x * Width), Width - 1),
        min((uint)(AtlasUV.y * Height), Height - 1));
    const float4 RawSample = ShadowDebugTexture.Load(int3(PixelCoord, 0));
    const float RawDepth = RawSample.r;

    if (ShadowDepthPreviewMode == 0)
    {
        return float4(RawDepth, 0.0, 0.0, 1.0);
    }

    if (ShadowDepthPreviewMode == 2)
    {
        float3 VisualColor = float3(RawSample.r, RawSample.g, 0.0f);
        if (ShadowFilterMethod == 3)
        {
            const float Encoded = max(RawSample.r, 1e-6f);
            const float Linear01 = saturate(-log(Encoded) / max(ShadowESMExponent, 0.01f));
            VisualColor = float3(saturate(pow(1.0f - Linear01, 0.55f)), RawSample.r, 0.0f);
        }
        else
        {
            VisualColor = saturate(VisualColor);
        }
        return float4(VisualColor, 1.0f);
    }

    float3 WorldNear = ReconstructWorld(Input.UV, 0.0);
    float3 WorldFar = ReconstructWorld(Input.UV, 1.0);
    float3 WorldPoint = ReconstructWorld(Input.UV, RawDepth);

    const float RayLength = max(length(WorldFar - WorldNear), 1e-6);
    const float Linear01 = saturate(length(WorldPoint - WorldNear) / RayLength);
    const float Visual = saturate(pow(1.0 - Linear01, 0.55));
    return float4(Visual, Visual, Visual, 1.0);
}
