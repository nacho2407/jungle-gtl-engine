#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

Texture2D SubUVAtlas : register(t0);
SamplerState SubUVSampler : register(s0);

PS_Input_Tex VS(VS_Input_PT input)
{
    PS_Input_Tex output;
    output.position = ApplyVP(input.position);
    output.texcoord = input.texcoord;
    return output;
}

float4 PS(PS_Input_Tex input) : SV_TARGET
{
    float4 col = SubUVAtlas.Sample(SubUVSampler, input.texcoord);
    // 두 가지 형식을 동시에 지원:
    //  - 파티클 atlas (R=intensity, A=1 또는 R=A): R 채널이 0에 가까우면 배경
    //  - 컬러 PNG 아이콘 (RGBA, straight alpha): A 가 0.5 미만이면 배경(헤일로 컷)
    if (!bIsWireframe && (col.a < 0.5f || col.r < 0.1f))
        discard;

    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}
