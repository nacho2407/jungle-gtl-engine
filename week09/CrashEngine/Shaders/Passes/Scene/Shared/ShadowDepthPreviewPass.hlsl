cbuffer ShadowDebugPreviewCB : register(b2)
{
    float4x4 InvViewProj;
    uint ShadowDepthPreviewMode;
    float3 ShadowDebugPreviewPadding;
};

Texture2D<float> ShadowDepthTexture : register(t0);

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
    ShadowDepthTexture.GetDimensions(Width, Height);
    const uint2 PixelCoord = uint2(
        min((uint)(Input.UV.x * Width), Width - 1),
        min((uint)(Input.UV.y * Height), Height - 1));
    const float RawDepth = ShadowDepthTexture.Load(int3(PixelCoord, 0)).r;

    if (ShadowDepthPreviewMode == 0)
    {
        return float4(RawDepth, 0.0, 0.0, 1.0);
    }

    float3 WorldNear = ReconstructWorld(Input.UV, 0.0);
    float3 WorldFar = ReconstructWorld(Input.UV, 1.0);
    float3 WorldPoint = ReconstructWorld(Input.UV, RawDepth);

    const float RayLength = max(length(WorldFar - WorldNear), 1e-6);
    const float Linear01 = saturate(length(WorldPoint - WorldNear) / RayLength);
    const float Visual = saturate(pow(1.0 - Linear01, 0.55));
    return float4(Visual, Visual, Visual, 1.0);
}
