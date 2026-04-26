/*
    NonLitViewMode.hlsl는 에디터 뷰 모드용 셰이딩 분기를 처리합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 텍스처/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언하는 추가 리소스: t0, s0, b2
*/

/*
    Available preprocessor defines:
    - NON_LIT_VIEW_SCENE_DEPTH
    - NON_LIT_VIEW_WORLD_NORMAL
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Surface/SurfacePacking.hlsli"
#include "../../../Resources/SystemResources.hlsl"

#if defined(NON_LIT_VIEW_WORLD_NORMAL)
Texture2D NormalTexture : register(t0);
SamplerState LinearClampSampler : register(s0);
#endif

#if defined(NON_LIT_VIEW_SCENE_DEPTH)
cbuffer SceneDepthParams : register(b2)
{
    float Exponent;
    float NearClip;
    float FarClip;
    float Range;
    uint Mode;
    float3 _Padding;
}
#endif

PS_Input_UV VS(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS(PS_Input_UV Input) : SV_TARGET
{
#if defined(NON_LIT_VIEW_SCENE_DEPTH)
    int2 Coord = int2(Input.position.xy);
    float Depth = SceneDepth.Load(int3(Coord, 0)).r;

    if (Depth <= 0.0000001f)
    {
        return float4(0, 0, 0, 1);
    }

    float LinearZ = (NearClip * FarClip) / (Depth * (FarClip - NearClip) + NearClip);
    float Value = 0.0f;

    if (Mode == 1)
    {
        Value = frac((LinearZ - NearClip) / max(0.1f, Range));
    }
    else
    {
        Value = saturate((LinearZ - NearClip) / max(0.1f, Range));
    }

    Value = pow(Value, Exponent);
    return float4(Value.xxx, 1.0f);
#elif defined(NON_LIT_VIEW_WORLD_NORMAL)
    float4 EncodedNormal = NormalTexture.Sample(LinearClampSampler, Input.uv);
    float3 Normal = DecodeNormal(EncodedNormal);
    return float4(Normal * 0.5f + 0.5f, 1.0f);
#else
    return float4(0, 0, 0, 1);
#endif
}
