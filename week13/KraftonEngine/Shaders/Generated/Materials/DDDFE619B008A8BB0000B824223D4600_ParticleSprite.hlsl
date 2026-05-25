// Generated from Content/Material/Material_Emitter0.mat
// Domain: ParticleSprite

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

struct FMaterialPixelInput
{
    float2 UV0;
    float2 UV1;
    float2 UV2;
    float4 ParticleColor;
    float4 VertexColor;
    float Time;
};

struct FMaterialResult
{
    float3 Color;
    float3 Emissive;
    float Opacity;
    float2 UVOffset;
};

Texture2D Tex_Diffuse : register(t0);

FMaterialResult EvaluateMaterial(FMaterialPixelInput Input)
{
    float4 n_3 = Tex_Diffuse.Sample(LinearWrapSampler, Input.UV0);
    float4 n_12 = Input.ParticleColor;
    float3 n_19 = ((n_3).rgb * (n_12).rgb);
    float n_23 = ((n_3).a * (n_12).a);
    FMaterialResult Result;
    Result.Color = n_19;
    Result.Emissive = float3(0, 0, 0);
    Result.Opacity = n_23;
    Result.UVOffset = float2(0, 0);
    return Result;
}


struct PS_Input_MaterialParticle
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR;
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
    output.position = mul(float4(worldPos, 1.0f), mul(View, Projection));
    output.texcoord = quad.cornerUV + 0.5f;
    output.color = inst.color;
    return output;
}

float4 PS(PS_Input_MaterialParticle input) : SV_TARGET
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0 = input.texcoord;
    MaterialInput.UV1 = float2(0, 0);
    MaterialInput.UV2 = float2(0, 0);
    MaterialInput.ParticleColor = input.color;
    MaterialInput.VertexColor = input.color;
    MaterialInput.Time = Time;

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    float4 FinalColor = float4(Result.Color + Result.Emissive, Result.Opacity);
    clip(FinalColor.a - 0.01f);
    return FinalColor;
}
