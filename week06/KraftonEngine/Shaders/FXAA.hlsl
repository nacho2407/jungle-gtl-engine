#include "Common/ConstantBuffers.hlsl"

Texture2D SceneColorTex : register(t0);
SamplerState SceneColorSampler : register(s0);

struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float FxaaLuma(float3 rgb)
{
    return rgb.r + rgb.g * (0.587f / 0.299f);
}

float4 SampleColor(float2 uv)
{
    return SceneColorTex.SampleLevel(SceneColorSampler, uv, 0.0f);
}

float SampleLuma(float2 uv)
{
    return FxaaLuma(SampleColor(uv).rgb);
}

float ComputeSubpixBlend(
    float lumaM,
    float lumaN, float lumaS,
    float lumaE, float lumaW,
    float localRange)
{
    // localRange이 너무 작으면 subpixel 블렌드할 필요가 없다고 판단
    if (FXAA_SUBPIX == 0 || localRange <= 1e-6f)
        return 0.0f;

    float lowpassLuma = 0.25f * (lumaN + lumaS + lumaE + lumaW);
    float pixelContrast = abs(lowpassLuma - lumaM);

    float ratio = saturate(pixelContrast / localRange);
    
    float blend = saturate((ratio - FXAA_SUBPIX_TRIM) / max(1.0f - FXAA_SUBPIX_TRIM, 1e-5f));
    blend = min(blend, FXAA_SUBPIX_CAP);

    return blend;
}

void ComputeEdgeOrientation(
    float lumaNW, float lumaN, float lumaNE,
    float lumaW, float lumaM, float lumaE,
    float lumaSW, float lumaS, float lumaSE,
    out bool bHorizontal)
{
    // 2차 차분(a - 2b + c)을 이용해서 가로 / 세로 edge 판단
    float edgeHorz =
        abs(lumaNW - 2.0f * lumaW + lumaSW) +
        abs(lumaN - 2.0f * lumaM + lumaS) +
        abs(lumaNE - 2.0f * lumaE + lumaSE);

    float edgeVert =
        abs(lumaNW - 2.0f * lumaN + lumaNE) +
        abs(lumaW - 2.0f * lumaM + lumaE) +
        abs(lumaSW - 2.0f * lumaS + lumaSE);
    
    bHorizontal = (edgeHorz >= edgeVert);
}

void SearchEdgeSpan(
    float2 uvStart,
    float2 searchStep,
    float referenceLuma,
    float gradientThreshold,
    out float negDist,
    out float posDist)
{
    float2 uvNeg = uvStart - searchStep;
    float2 uvPos = uvStart + searchStep;

    bool doneNeg = false;
    bool donePos = false;

    negDist = 1.0f;
    posDist = 1.0f;

    [loop]
    for (uint i = 0; i < FXAA_SEARCH_STEPS; ++i)
    {
        if (!doneNeg)
        {
            float lumaNeg = SampleLuma(uvNeg);
            doneNeg = abs(lumaNeg - referenceLuma) >= gradientThreshold;

            if (!doneNeg)
            {
                uvNeg -= searchStep;
                negDist += 1.0f;
            }
        }

        if (!donePos)
        {
            float lumaPos = SampleLuma(uvPos);
            donePos = abs(lumaPos - referenceLuma) >= gradientThreshold;

            if (!donePos)
            {
                uvPos += searchStep;
                posDist += 1.0f;
            }
        }

        if (doneNeg && donePos)
            break;
    }
}

float4 PS(PS_Input input) : SV_TARGET
{
    const float2 px = FxaaRcpFrame;
    
    float4 colorM = SampleColor(input.uv);
    float4 colorN = SampleColor(input.uv + float2(0.0f, -px.y));
    float4 colorS = SampleColor(input.uv + float2(0.0f, px.y));
    float4 colorE = SampleColor(input.uv + float2(px.x, 0.0f));
    float4 colorW = SampleColor(input.uv + float2(-px.x, 0.0f));

    float4 colorNW = SampleColor(input.uv + float2(-px.x, -px.y));
    float4 colorNE = SampleColor(input.uv + float2(px.x, -px.y));
    float4 colorSW = SampleColor(input.uv + float2(-px.x, px.y));
    float4 colorSE = SampleColor(input.uv + float2(px.x, px.y));

    float lumaM = FxaaLuma(colorM.rgb);
    float lumaN = FxaaLuma(colorN.rgb);
    float lumaS = FxaaLuma(colorS.rgb);
    float lumaE = FxaaLuma(colorE.rgb);
    float lumaW = FxaaLuma(colorW.rgb);
    float lumaNW = FxaaLuma(colorNW.rgb);
    float lumaNE = FxaaLuma(colorNE.rgb);
    float lumaSW = FxaaLuma(colorSW.rgb);
    float lumaSE = FxaaLuma(colorSE.rgb);
    
    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float localRange = rangeMax - rangeMin;

    float localThreshold = max(rangeMax * FXAA_EDGE_THRESHOLD, FXAA_EDGE_THRESHOLD_MIN);

    if (localRange < localThreshold)
        return colorM;
    
    float subpixBlend = ComputeSubpixBlend(lumaM, lumaN, lumaS, lumaE, lumaW, localRange);
    
    bool bHorizontal = false;
    ComputeEdgeOrientation(
        lumaNW, lumaN, lumaNE,
        lumaW, lumaM, lumaE,
        lumaSW, lumaS, lumaSE,
        bHorizontal);
    
    float lumaNeg = bHorizontal ? lumaN : lumaW;
    float lumaPos = bHorizontal ? lumaS : lumaE;

    float gradNeg = abs(lumaNeg - lumaM);
    float gradPos = abs(lumaPos - lumaM);

    bool bUseNeg = (gradNeg >= gradPos);

    float2 pairStep = bHorizontal ? float2(0.0f, px.y) : float2(px.x, 0.0f);
    if (bUseNeg)
        pairStep = -pairStep;

    float pairedLuma = bUseNeg ? lumaNeg : lumaPos;
    float referenceLuma = 0.5f * (lumaM + pairedLuma);
    
    float2 searchStep = bHorizontal ? float2(px.x, 0.0f) : float2(0.0f, px.y);

    float gradient = max(gradNeg, gradPos);
    float gradientThreshold = gradient * FXAA_SEARCH_THRESHOLD;

    float2 uvStart = input.uv + pairStep * 0.5f;

    float negDist = 0.0f;
    float posDist = 0.0f;
    SearchEdgeSpan(uvStart, searchStep, referenceLuma, gradientThreshold, negDist, posDist);
    
    float spanLength = negDist + posDist;
    float pixelOffset = 0.0f;
    if (spanLength > 1e-5f)
    {
        pixelOffset = (posDist - negDist) / spanLength;
    }

    float2 finalOffset = pairStep * pixelOffset;

    float4 edgeColor = SampleColor(input.uv + finalOffset);
    
    if (FXAA_SUBPIX != 0)
    {
        float4 lowpassColor =
            (colorM + colorN + colorS + colorE + colorW + colorNW + colorNE + colorSW + colorSE) / 9.0f;

        edgeColor = lerp(edgeColor, lowpassColor, subpixBlend);
    }
    
    edgeColor.a = colorM.a;
    
    return edgeColor;
}
