#include "../Common.hlsl"


/*
1/3 – too little
1/4 – low quality
1/8 – high quality
1/16 – overkill
*/
#define FXAA_EDGE_THRESHOLD (1.0 / 8.0)

/*
1/32 – visible limit
1/16 – high quality
1/12 – upper limit (start of visible unfiltered edges)
*/
#define FXAA_EDGE_THRESHOLD_MIN (1.0 / 16.0)

/*
Toggle subpix filtering.
0 – turn off
1 – turn on
2 – turn on force full (ignore FXAA_SUBPIX_TRIM and CAP) => will blur the image
*/
#define FXAA_SUBPIX 0

/*
Controls removal of sub-pixel aliasing.
1/2 – low removal
1/3 – medium removal
1/4 – default removal
1/8 – high removal
0 – complete removal
*/
#define FXAA_SUBPIX_TRIM (1.0 / 4.0)

/*
Insures fine detail is not completely removed.
This partly overrides FXAA_SUBPIX_TRIM.
3/4 – default amount of filtering
7/8 – high amount of filtering
1 – no capping of filtering
*/
#define FXAA_SUBPIX_CAP (3.0 / 4.0)

#define FXAA_SUBPIX_TRIM_SCALE (1.0 / (1.0 - FXAA_SUBPIX_TRIM))
#define FXAA_SEARCH_STEPS 8
#define FXAA_SEARCH_ACCELERATION 1

// Input
Texture2D FinalSceneColor : register(t0);
SamplerState SampleState : register(s0);

cbuffer FXAAConstants : register(b10)
{
    float2 InvResolution; // (1/Width, 1/Height)
    uint Enabled; // 0: off, 1: on
    float Padding;
}

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

// Fullscreen Triangle
VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;

    float2 pos;

    if (vertexID == 0)
        pos = float2(-1.0f, -1.0f);
    else if (vertexID == 1)
        pos = float2(-1.0f, 3.0f);
    else
        pos = float2(3.0f, -1.0f);

    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}

float GetLuma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float FxaaLuma(float3 rgb)
{
    // 곱셈 3개 => 곱셈 1개, 덧셈 1개 최적화
    // 파랑색은 보통 게임에서 aliasing 으로 나타나지 않는다는 가정
    return rgb.y * (0.587 / 0.299) + rgb.x;
}

float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);
    float2 uv = (float2(ip) + 0.5) * InvResolution;

    float3 rgbM = FinalSceneColor.Load(int3(ip, 0)).rgb;
    
    if (Enabled == 0)
        return float4(rgbM, 1);

    float3 rgbN = FinalSceneColor.Load(int3(ip + int2(0, -1), 0)).rgb;
    float3 rgbS = FinalSceneColor.Load(int3(ip + int2(0, 1), 0)).rgb;
    float3 rgbW = FinalSceneColor.Load(int3(ip + int2(-1, 0), 0)).rgb;
    float3 rgbE = FinalSceneColor.Load(int3(ip + int2(1, 0), 0)).rgb;

    /*
        밝기 계산
    */
    float lumaM = FxaaLuma(rgbM);
    float lumaN = FxaaLuma(rgbN);
    float lumaS = FxaaLuma(rgbS);
    float lumaW = FxaaLuma(rgbW);
    float lumaE = FxaaLuma(rgbE);

    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float range = rangeMax - rangeMin;

    if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
    {
        // Edge 가 아닌 경우 Anti-aliasing X
        return float4(rgbM, 1);
    }

    /*
        Edge 에 따라 AA 적용할 direction 결정
    */
    float2 dir;
    dir.x = -((lumaN + lumaS) - 2.0 * lumaM);
    dir.y = ((lumaW + lumaE) - 2.0 * lumaM);

    /*
        노이즈 제거
    */
    
    // 밝기 평균 기반 감쇠
    float dirReduce = max(
        (lumaN + lumaS + lumaW + lumaE) * 0.25 * 0.0312,
        0.0078125);
    // 방향 정규화 
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    // 너무 멀리 샘플링 하는거 방지
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * InvResolution;

    // 2 샘플 평균
    float3 rgbA =
        0.5 * (
            FinalSceneColor.Sample(SampleState, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
            FinalSceneColor.Sample(SampleState, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    // 더 넓게 blur
    float3 rgbB =
        rgbA * 0.5 +
        0.25 * (
            FinalSceneColor.Sample(SampleState, uv + dir * -0.5).rgb +
            FinalSceneColor.Sample(SampleState, uv + dir * 0.5).rgb);

    float lumaB = FxaaLuma(rgbB);

    if ((lumaB < rangeMin) || (lumaB > rangeMax))
    {
        // 과도한 blur 방지
        return float4(rgbA, 1);
    }

    return float4(rgbB, 1);
}
