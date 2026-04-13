#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

cbuffer PerShader1 : register(b2)
{

    float4 SectionColor;
    float4 DiffuseColor;
    uint bIsUVScroll;
    float3 _matPad;
};

cbuffer PerShader2 : register(b3)
{
    float3 lightDir;
};

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    output.color = input.color * SectionColor;

    float2 texcoord = input.texcoord;
    if (bIsUVScroll != 0)
    {
        texcoord.x += Time * 0.5f; // 가로 방향으로 스크롤 예시
    }
    output.texcoord = texcoord;

    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 texColor = g_txColor.Sample(g_Sample, input.texcoord);

    // Unbound SRV는 (0,0,0,0)을 반환 — 텍스처 미바인딩 시 white로 대체
    if (texColor.a < 0.001f)
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    //float3 lightDir = normalize(float3(1.0f, -1.0f, 1.0f));
    //float diffuse = max(dot(input.normal, -lightDir), 0.0f);
    //float ambient = 0.2f;

    float4 finalColor = texColor * input.color * DiffuseColor/* * (diffuse + ambient)*/;
    finalColor.a = texColor.a * input.color.a;

    return DiffuseColor;
    return float4(ApplyWireframe(finalColor.rgb), finalColor.a);
}
