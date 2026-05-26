// Generated from Content/Material/Material_Emitter0.mat
// Domain: ParticleSprite

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#define USE_FOG 1
#include "Common/Fog.hlsli"

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

Texture2D Tex_Diffuse : register(t0);
Texture2D Tex_SubUV : register(t6);

FMaterialResult EvaluateMaterial(FMaterialPixelInput Input)
{
    float n_100 = -0.200000f;
    float n_98 = 1.000000f;
    float2 n_133 = Input.UV0;
    float4 n_3 = Tex_Diffuse.Sample(LinearWrapSampler, n_133);
    float n_93 = lerp(n_100, n_98, (n_3).r);
    float n_179 = 0.000000f;
    float n_183 = 1.000000f;
    float n_172 = clamp(n_93, n_179, n_183);
    float2 n_39 = ((float2(fmod(floor(Input.SubImageIndex * 36), 6), floor(Input.SubImageIndex * 36 / 6)) + Input.UV0) * float2(1.0f/6, 1.0f/6));
    float4 n_43 = Tex_SubUV.Sample(LinearWrapSampler, n_39);
    float3 n_162 = (float3(n_172, n_172, n_172) + (n_43).rgb);
    float4 n_12 = Input.ParticleColor;
    float3 n_111 = (n_162 * (n_12).rgb);
    float4 n_60 = Input.DynamicParam;
    float n_71 = 1.000000f;
    float n_66 = ((n_60).r + n_71);
    float n_54 = pow((n_43).r, n_66);
    float n_84 = 0.000000f;
    float n_81 = 1.000000f;
    float n_75 = clamp(n_54, n_84, n_81);
    float n_87 = (n_75 * (n_12).a);
    float n_124 = 0.000000f;
    float n_126 = 1.000000f;
    float n_118 = clamp(n_87, n_124, n_126);
    FMaterialResult Result;
    Result.Color = float3(0, 0, 0);
    Result.Emissive = n_111;
    Result.Opacity = n_118;
    Result.UVOffset = float2(0, 0);
    return Result;
}


struct PS_Input_MaterialParticle
{
    float4 position       : SV_POSITION;
    float2 texcoord       : TEXCOORD0;
    float4 color          : COLOR;
    float  subImageIndex  : TEXCOORD1;
    float4 dynamicParam   : TEXCOORD2;
    float3 worldPos       : TEXCOORD3;
};

PS_Input_MaterialParticle VS(VS_Input_ParticleQuad quad, VS_Input_ParticleInstance inst)
{
    float sinR = sin(inst.rotation);
    float cosR = cos(inst.rotation);

    float2 rotUV = float2(
        quad.cornerUV.x * cosR - quad.cornerUV.y * sinR,
        quad.cornerUV.x * sinR + quad.cornerUV.y * cosR
    );

    float3 worldPos = inst.position
                    + FrameCameraRight * rotUV.x * inst.size
                    + FrameCameraUp * rotUV.y * inst.size;

    PS_Input_MaterialParticle output;
    output.position       = mul(float4(worldPos, 1.0f), mul(View, Projection));
    output.texcoord       = quad.cornerUV + 0.5f;
    output.color          = inst.color;
    output.subImageIndex  = inst.subImageIndex;
    output.dynamicParam   = inst.dynamicParam;
    output.worldPos       = worldPos;
    return output;
}

float4 PS(PS_Input_MaterialParticle input) : SV_TARGET
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
    float4 FinalColor = float4(Result.Color + Result.Emissive, Result.Opacity);
    clip(FinalColor.a - 0.01f);
    return ApplyFogTranslucent(FinalColor, input.worldPos, CameraWorldPos);
}
