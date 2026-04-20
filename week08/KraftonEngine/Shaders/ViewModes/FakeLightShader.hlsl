#include "../Common/Utils/Functions.hlsl"
#include "../Common/Geometry/VertexLayouts.hlsl"
#include "../Common/Resources/SystemSamplers.hlsl"

Texture2D g_txColor : register(t0);

// b2 (PerShader0)
cbuffer DecalBuffer : register(b2)
{
    float4x4 DecalWorldToLocal;
    float4 DecalColor;
}

PS_Input_Decal VS(VS_Input_PNCT input)
{
    PS_Input_Decal output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    output.position = mul(mul(worldPos, View), Projection);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    return output;
}

float4 PS(PS_Input_Decal input) : SV_TARGET
{
    float3 decalLocalPos = mul(float4(input.worldPos, 1.0f), DecalWorldToLocal).xyz;

    if (abs(decalLocalPos.x) > 0.5f || abs(decalLocalPos.y) > 0.5f || abs(decalLocalPos.z) > 0.5f)
    {
        discard;
    }

    float2 uv;
    uv.x = decalLocalPos.y + 0.5f;
    uv.y = 0.5f - decalLocalPos.z;

    float4 texColor = g_txColor.Sample(LinearWrapSampler, uv);
    if (texColor.a < 0.001f)
    {
        discard;
    }
    
    float4 finalColor = texColor * DecalColor;
    float distAlpha = saturate(0.5f - length(decalLocalPos));
    finalColor.a *= distAlpha;
    
    // Additive 블렌딩을 위해 Alpha를 RGB에 미리 곱함 (ONE, ONE 블렌딩 대응)
    return float4(finalColor.rgb * finalColor.a, 1.0f);
}
