#include "Common.hlsl"

struct VS_INPUT
{
    float3 p : POSITION;
    float3 n : NORMAL;
    float4 c : COLOR;
    float2 t : TEXTCOORD;
};

struct PS_INPUT
{
    float4 p : SV_POSITION;
    float3 n : NORMAL;
    float4 c : COLOR;
    float2 t : TEXTCOORD;
};

Texture2D    g_txColor : register(t0);
SamplerState g_Sample  : register(s0);

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.p = ApplyMVP(input.p);
    output.n = normalize(mul(input.n, (float3x3) Model));
    output.c = input.c * PrimitiveColor;
    output.t = input.t;
    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // 1. 텍스처에서 현재 UV의 픽셀 색상을 가져옵니다.
    float4 texColor = g_txColor.Sample(g_Sample, input.t);

    // Unbound SRV는 (0,0,0,0)을 반환 — 텍스처 미바인딩 시 white로 대체
    if (texColor.a < 0.001f)
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    // 2. 방향성 조명 (Directional Light) 연산
    // 빛의 방향 (임의로 우측 상단에서 쏘는 빛)
    float3 lightdir = normalize(float3(1.0f, 1.0f, -1.0f));
    
    // 픽셀의 노멀과 빛의 반대 방향을 내적(dot)하여 밝기를 구합니다. (0.0 ~ 1.0)
    float diffuse = max(dot(input.n, -lightdir), 0.0f);
    
    // 그림자 영역의 최소 밝기 (Ambient)
    float ambient = 0.2f;
    
    // 3. 최종 색상 연산 (텍스처 * 정점&오브젝트색상 * 조명밝기)
    float4 finalColor = texColor * input.c  * (diffuse + ambient);
    
    // 투명도(Alpha)는 조명 연산에서 제외하고 원본 값을 유지합니다.
    finalColor.a = texColor.a * input.c.a;
    
    return finalColor;
}
