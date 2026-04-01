cbuffer FOutlinePostProcessConstants : register(b0)
{
    float2 MaskTexelSize;
    float Thickness;
    float Padding0;
    float2 ViewMinUV;
    float2 ViewSizeUV;
    float4 OutlineColor;
};

Texture2D MaskTexture : register(t0);
SamplerState MaskSampler : register(s0);

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VSOutput VSMain(uint VertexId : SV_VertexID)
{
    VSOutput Out;

    if (VertexId == 0)
    {
        Out.Position = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        Out.UV = float2(0.0f, 1.0f);
    }
    else if (VertexId == 1)
    {
        Out.Position = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        Out.UV = float2(0.0f, -1.0f);
    }
    else
    {
        Out.Position = float4(3.0f, -1.0f, 0.0f, 1.0f);
        Out.UV = float2(2.0f, 1.0f);
    }

    return Out;
}

float SampleMask(float2 UV)
{
    return MaskTexture.Sample(MaskSampler, UV).a;
}

float4 PSMain(VSOutput In) : SV_TARGET
{
    float2 MaskUV = ViewMinUV + In.UV * ViewSizeUV;

    float Center = SampleMask(MaskUV);
    if (Center > 0.0f)
    {
        discard;
    }

    int Radius = (int) Thickness;

    float Edge = 0.0f;

    [loop]
    for (int Step = 1; Step <= Radius; ++Step)
    {
        float PixelOffset = (float)Step;
        float2 OffsetX = float2(MaskTexelSize.x * PixelOffset, 0.0f);
        float2 OffsetY = float2(0.0f, MaskTexelSize.y * PixelOffset);
        float2 OffsetXY =
            float2(MaskTexelSize.x * PixelOffset, MaskTexelSize.y * PixelOffset);

        Edge = max(Edge, SampleMask(MaskUV + OffsetX));
        Edge = max(Edge, SampleMask(MaskUV - OffsetX));
        Edge = max(Edge, SampleMask(MaskUV + OffsetY));
        Edge = max(Edge, SampleMask(MaskUV - OffsetY));
        Edge = max(Edge, SampleMask(MaskUV + OffsetXY));
        Edge = max(Edge, SampleMask(MaskUV - OffsetXY));
        Edge = max(Edge, SampleMask(MaskUV + float2(OffsetXY.x, -OffsetXY.y)));
        Edge = max(Edge, SampleMask(MaskUV + float2(-OffsetXY.x, OffsetXY.y)));
    }

    if (Edge <= 0.0f)
    {
        discard;
    }

    return float4(OutlineColor.rgb, OutlineColor.a * Edge);
}
