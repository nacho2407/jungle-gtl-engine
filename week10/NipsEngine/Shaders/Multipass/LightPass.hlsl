#include "../Common.hlsl"

// GBuffer
Texture2D SceneColor : register(t0);
Texture2D SceneNormal : register(t1);
Texture2D SceneDepth : register(t2);
Texture2D SceneWorldPos : register(t3);

SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

struct FLightData
{
    float3 WorldPos;
    float  Radius;
    float3 Color;
    float  Intensity;
    float  RadiusFalloff;
    float  Padding[2];
};

StructuredBuffer<FLightData> Lights : register(t4);

cbuffer LightPassConstants : register(b7)
{
    float3 CameraWorldPos;
    uint LightCount;
    
    uint ViewMode;
    uint   WorldLit;
    float2 Padding;
};

// Fullscreen Triangle VS
VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;

    float2 pos;

    // Triangular quad
    if (vertexID == 0)
    {
        pos = float2(-1.0f, -1.0f);
    }
    else if (vertexID == 1)
    {
        pos = float2(-1.0f, 3.0f);
    }
    else // 2
    {
        pos = float2(3.0f, -1.0f);
    }

    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}

// PS
float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);
    float3 albedo = SceneColor.Load(int3(ip, 0)).rgb;
    float4 normal = SceneNormal.Load(int3(ip, 0));
    float depth = SceneDepth.Load(int3(ip, 0)).r;
    float3 worldPos = SceneWorldPos.Load(int3(ip, 0)).rgb;
    
    if (ViewMode == 3) // Depth View Mode
    {
        if (depth >= 1.0f)
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        
        float visual;
        
        // HLSL에서 row-major로 들어온 것을 반영해 원근 투영
        if (Projection[3][3] < 0.5f)
        {
            // return float4(1.0f, 0.0f, 0.0f, 1.0f);
            float A = Projection[0][2];
            float B = Projection[3][2];
            float zView = abs(B / (depth - A));
            
            float DepthDensity = 0.05f; // 상황에 따라 0.01f ~ 0.001f 등 조절
            
            // 거리가 멀어질수록 exp 안의 음수값이 커져 visual이 0(검정)에 수렴
            visual = saturate(exp(-zView * DepthDensity));

            // 흰색 = 가까움, 검정 = 멀리 (언리얼 Depth 뷰 모드와 동일)
        }
        else // 직교 투영
        {
            visual = 1.0f - depth;
        }
        
        return float4(visual, visual, visual, 1.0f);
    }
    
    // SceneNormalRTV is cleared every frame using ClearColor = { 0.25f, 0.25f, 0.25f, 1.0f },
    // which is the default background color. (D3DDevice.cpp line 40)
    if (normal.a < 1e-4)
    {
        discard;
    }

    // Emissive surfaces (normal.a == 2) skip lighting and glow with their own color
    if (normal.a > 1.5f)
    {
        return float4(albedo, 1.0f);
    }

    float3 light_accumulation = float3(0, 0, 0);
    for (uint i = 0; i < LightCount; i++)
    {
        // accumulate Lights[i] contribution;
        float3 toLight     = Lights[i].WorldPos - worldPos;
        float  dist        = length(toLight);
        float  attenuation = saturate(1.0 - (dist / Lights[i].Radius));
        attenuation        = pow(attenuation, Lights[i].RadiusFalloff);
        
        float3 N     = normalize(normal.rgb * 2.0f - 1.0f);
        float3 L     = normalize(toLight);
        float  NdotL = saturate(dot(N, L));
        
        light_accumulation += Lights[i].Color * Lights[i].Intensity * NdotL * attenuation;
    }
    
    float3 ambience = albedo * 0.25f;
    float3 final_rgb = WorldLit == 0 ? clamp(albedo * light_accumulation + ambience, 0.f, 1.f) : albedo;
    return float4(final_rgb, 1.0f);
    
    
    //return float4(normal);
    // float visual = 1.0f - depth;
    // return float4(visual, visual, visual, 1.0f);
}