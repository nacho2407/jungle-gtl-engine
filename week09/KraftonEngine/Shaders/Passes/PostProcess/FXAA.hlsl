// FXAA.hlsl
#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"
#include "../../Common/Resources/SystemSamplers.hlsl"

cbuffer FXAABuffer : register(b2)
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float2 _Pad;
};

// SceneColor (t11) is declared in Common/SystemResources.hlsl
#define ColorTex SceneColor
#define Sampler LinearClampSampler

#define SEARCH_STEPS        8       // 최대 탐색 스텝 수
#define SEARCH_THRESHOLD    0.25    // 끝단 판별 임계값 (1/4)
#define SUBPIX_CAP          0.75    // 서브픽셀 블렌드 상한 (3/4)
#define SUBPIX_TRIM         0.25    // 서브픽셀 제거 하한 (1/4)


float GetLuma(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 color = ColorTex.SampleLevel(Sampler, uv, 0);
    float lumaM = GetLuma(color.rgb);
    
    uint Width, Height;
    ColorTex.GetDimensions(Width, Height);
    
    float2 TexelSize = { 1.0f / Width, 1.0f / Height };
    
    float lumaN = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(0, -TexelSize.y), 0).rgb);
    float lumaS = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(0, TexelSize.y), 0).rgb);
    float lumaW = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(-TexelSize.x, 0), 0).rgb);
    float lumaE = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(TexelSize.x, 0), 0).rgb);
    
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float lumaRange = lumaMax - lumaMin;
    
    if (lumaRange < max(EdgeThresholdMin, lumaMax * EdgeThreshold))
        return color;
    
    float lumaL = (lumaN + lumaS + lumaW + lumaE) * 0.25;
    float rangeL = abs(lumaL - lumaM);
    float blendL = max(0.0, (rangeL / lumaRange) - SUBPIX_TRIM) * (1.0 / (1.0 - SUBPIX_TRIM));
    blendL = min(SUBPIX_CAP, blendL);
    
    float lumaNW = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(-TexelSize.x, -TexelSize.y), 0).rgb);
    float lumaNE = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(TexelSize.x, -TexelSize.y), 0).rgb);
    float lumaSW = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(-TexelSize.x, TexelSize.y), 0).rgb);
    float lumaSE = GetLuma(ColorTex.SampleLevel(Sampler, uv + float2(TexelSize.x, TexelSize.y), 0).rgb);
    
    float edgeVert =
        abs(0.25 * lumaNW - 0.5 * lumaN + 0.25 * lumaNE) +
        abs(0.50 * lumaW - 1.0 * lumaM + 0.50 * lumaE) +
        abs(0.25 * lumaSW - 0.5 * lumaS + 0.25 * lumaSE);

    float edgeHorz =
        abs(0.25 * lumaNW - 0.5 * lumaW + 0.25 * lumaSW) +
        abs(0.50 * lumaN - 1.0 * lumaM + 0.50 * lumaS) +
        abs(0.25 * lumaNE - 0.5 * lumaE + 0.25 * lumaSE);
    
    bool horzSpan = edgeHorz >= edgeVert;
    
    float2 stepDir = horzSpan ? float2(0, TexelSize.y) : float2(TexelSize.x, 0);
    float lumaNeg = horzSpan ? lumaN : lumaW;
    float lumaPos = horzSpan ? lumaS : lumaE;
    
    float gradNeg = abs(lumaNeg - lumaM);
    float gradPos = abs(lumaPos - lumaM);
    
    bool negSide = gradNeg >= gradPos;
    float gradScaled = 0.25 * max(gradNeg, gradPos);
    float lumaLocalAve = negSide ? (lumaNeg + lumaM) * 0.5
                                 : (lumaPos + lumaM) * 0.5;
    float2 uvEdge = negSide ? uv - stepDir * 0.5
                             : uv + stepDir * 0.5;
    
 
    float2 searchDir = horzSpan ? float2(TexelSize.x, 0) : float2(0, TexelSize.y);

    float2 posN = uvEdge - searchDir;
    float2 posP = uvEdge + searchDir;

    float lumaEndN = GetLuma(ColorTex.SampleLevel(Sampler, posN, 0).rgb);
    float lumaEndP = GetLuma(ColorTex.SampleLevel(Sampler, posP, 0).rgb);
    lumaEndN -= lumaLocalAve;
    lumaEndP -= lumaLocalAve;

    bool doneN = abs(lumaEndN) >= gradScaled;
    bool doneP = abs(lumaEndP) >= gradScaled;
    
    for (int i = 0; i < SEARCH_STEPS; i++)
    {
        if (!doneN)
            posN -= searchDir;
        if (!doneP)
            posP += searchDir;

        if (!doneN)
        {
            lumaEndN = GetLuma(ColorTex.SampleLevel(Sampler, posN, 0).rgb);
            lumaEndN -= lumaLocalAve;
        }
        if (!doneP)
        {
            lumaEndP = GetLuma(ColorTex.SampleLevel(Sampler, posP, 0).rgb);
            lumaEndP -= lumaLocalAve;
        }

        doneN = doneN || (abs(lumaEndN) >= gradScaled);
        doneP = doneP || (abs(lumaEndP) >= gradScaled);

        if (doneN && doneP)
            break;
    }
    
    float distN = horzSpan ? (uv.x - posN.x) : (uv.y - posN.y);
    float distP = horzSpan ? (posP.x - uv.x) : (posP.y - uv.y);

    bool closerN = distN < distP;
    float spanLen = distN + distP;
    float pixelOffset = 0.5 - (closerN ? distN : distP) / spanLen;

    bool lumaMLower = lumaM < lumaLocalAve;
    bool goodSpanN = (lumaEndN < 0.0) != lumaMLower;
    bool goodSpanP = (lumaEndP < 0.0) != lumaMLower;
    bool goodSpan = closerN ? goodSpanN : goodSpanP;
    float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
        
    float finalOffset = max(pixelOffsetGood, blendL);

    float2 finalUV = uv;
    if (horzSpan)
        finalUV.y += finalOffset * (negSide ? -TexelSize.y : TexelSize.y);
    else
        finalUV.x += finalOffset * (negSide ? -TexelSize.x : TexelSize.x);

    return ColorTex.SampleLevel(Sampler, finalUV, 0);
}