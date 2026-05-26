// Generated from Content/Material/Auto/BasicShapeMaterial_Emitter0.mat
// Domain: ParticleMesh

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#define USE_FOG 1
#include "Common/Fog.hlsli"
#include "Common/ForwardLightData.hlsli"

struct FMaterialPixelInput
{
    float2 UV0;
    float2 UV1;
    float2 UV2;
    float4 ParticleColor;
    float4 VertexColor;
    float  Time;
    float  SubImageIndex;
    float4 DynamicParam;
};

struct FMaterialResult
{
    float3 Color;
    float3 Emissive;
    float Opacity;
    float2 UVOffset;
};

FMaterialResult EvaluateMaterial(FMaterialPixelInput Input)
{
    float4 n_1 = Input.ParticleColor;
    FMaterialResult Result;
    Result.Color = (n_1).xyz;
    Result.Emissive = float3(0, 0, 0);
    Result.Opacity = (n_1).x;
    Result.UVOffset = float2(0, 0);
    return Result;
}


struct PS_Input_MaterialMeshParticle
{
    float4 position       : SV_POSITION;
    float3 normal         : NORMAL;
    float2 texcoord       : TEXCOORD0;
    float4 color          : COLOR;
    float  subImageIndex  : TEXCOORD1;
    float4 dynamicParam   : TEXCOORD2;
    float3 worldPos       : TEXCOORD3;
};

PS_Input_MaterialMeshParticle VS(VS_Input_PNCT vert, VS_Input_MeshParticleInstance inst)
{
    float4 worldPos = mul(float4(vert.position, 1.0f), inst.transform);
    // 비균일 스케일에서 노말 왜곡 방지: 역전치 행렬 사용
    float3x3 M = (float3x3)inst.transform;
    float3x3 invTransM = transpose(float3x3(
        cross(M[1], M[2]),
        cross(M[2], M[0]),
        cross(M[0], M[1])
    ));
    float3 worldNormal = mul(vert.normal, invTransM);

    PS_Input_MaterialMeshParticle output;
    output.position       = mul(worldPos, mul(View, Projection));
    output.normal         = normalize(worldNormal);
    output.texcoord       = vert.texcoord;
    output.color          = vert.color * inst.color;
    output.subImageIndex  = inst.subImageIndex;
    output.dynamicParam   = inst.dynamicParam;
    output.worldPos       = worldPos.xyz / worldPos.w;
    return output;
}

float4 PS(PS_Input_MaterialMeshParticle input) : SV_TARGET
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0           = input.texcoord;
    MaterialInput.UV1           = float2(0, 0);
    MaterialInput.UV2           = float2(0, 0);
    MaterialInput.ParticleColor = input.color;
    MaterialInput.VertexColor   = input.color;
    MaterialInput.Time          = Time;
    MaterialInput.SubImageIndex = input.subImageIndex;
    MaterialInput.DynamicParam  = input.dynamicParam;

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    float3 BaseColor = Result.Color;

    float3 N = normalize(input.normal);
    float3 ambient = AmbientLight.Color.rgb * AmbientLight.Intensity;
    float NdotL = saturate(dot(N, -DirectionalLight.Direction));
    float3 directional = DirectionalLight.Color.rgb * DirectionalLight.Intensity * NdotL;
    float3 lighting = saturate(ambient + directional);
    BaseColor = BaseColor * lighting;

    float4 FinalColor = float4(BaseColor + Result.Emissive, Result.Opacity);
    clip(FinalColor.a - 0.01f);
    return ApplyFogTranslucent(FinalColor, input.worldPos, CameraWorldPos);
}
