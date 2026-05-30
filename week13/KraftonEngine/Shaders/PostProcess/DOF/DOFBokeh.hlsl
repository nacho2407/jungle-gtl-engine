#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

struct BokehVSOutput
{
    float4 position : SV_POSITION;
    float2 local : TEXCOORD0;
    float3 color : TEXCOORD1;
    float signedCoC : TEXCOORD2;
    float radiusPixels : TEXCOORD3;
};

BokehVSOutput VS(uint vertexID : SV_VertexID)
{
    static const float2 Corners[6] =
    {
        float2(-1.0f, -1.0f),
        float2( 1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f, -1.0f),
        float2( 1.0f,  1.0f)
    };

    uint vertexInQuad = vertexID % 6;
    uint spriteIndex = vertexID / 6;
    uint halfWidth = max((uint)round(1.0f / DOFInvHalfResolution.x), 1u);
    uint sourceX = spriteIndex % halfWidth;
    uint sourceY = spriteIndex / halfWidth;

    float4 source = DOFColorCoCTex.Load(int3((int)sourceX, (int)sourceY, 0));
    float sourceCoC = abs(source.a);
    float highlight = BokehHighlightRatio(source.rgb);
    float radiusPixels = sourceCoC * max(DOFBokehRadiusScale, 0.0f);

    if (radiusPixels < 1.0f || highlight <= 0.0f || DOFBokehIntensity <= 0.0f)
    {
        radiusPixels = 0.0f;
    }

    float2 centerUV = (float2((float)sourceX, (float)sourceY) + 0.5f) * DOFInvHalfResolution;
    float2 local = Corners[vertexInQuad];
    float2 ndc = centerUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    ndc += local * radiusPixels * float2(DOFInvHalfResolution.x * 2.0f, -DOFInvHalfResolution.y * 2.0f);

    BokehVSOutput output;
    output.position = float4(ndc, 0.0f, 1.0f);
    output.local = local;
    output.color = source.rgb * highlight;
    output.signedCoC = source.a;
    output.radiusPixels = radiusPixels;
    return output;
}

float4 PS(BokehVSOutput input) : SV_Target
{
    if (input.radiusPixels < 1.0f)
    {
        discard;
    }

    float apertureBlades = clamp(round(DOFApertureBladeCount), 3.0f, 16.0f);
    float angle = atan2(input.local.y, input.local.x);
    float distanceFromCenter = length(input.local);
    float apertureRadius = PolygonBoundaryRadius(angle, apertureBlades);

    float edge = saturate((apertureRadius - distanceFromCenter) * max(input.radiusPixels, 1.0f));
    edge = smoothstep(0.0f, 1.0f, edge);

    if (edge <= 0.0f)
    {
        discard;
    }

    float cocWeight = saturate(abs(input.signedCoC) / max(DOFMaxCoCRadius, 0.001f));
    float3 color = input.color * edge * cocWeight * max(DOFBokehIntensity, 0.0f);
    return float4(color, 0.0f);
}
