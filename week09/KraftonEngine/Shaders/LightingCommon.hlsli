#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

float3 GetMainLightDirection()
{
    return normalize(float3(0.4f, -0.8f, 0.2f));
}

float3 GetMainLightColor()
{
    return float3(1.0f, 0.98f, 0.95f);
}

float ComputeLambertTerm(float3 Normal)
{
    return saturate(dot(normalize(Normal), -GetMainLightDirection()));
}

float4 ComputeGouraudLighting(float4 BaseColor, float4 GouraudL)
{
    return float4(BaseColor.rgb * GouraudL.rgb, BaseColor.a);
}

float ComputeGouraudLightingFactor(float3 Normal)
{
    return 0.2f + ComputeLambertTerm(Normal) * 0.8f;
}

float3 ReconstructWorldPositionFromSceneDepth(float2 UV)
{
    float Depth = SceneDepth.Sample(PointClampSampler, UV).r;
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InvViewProj);
    return World.xyz / max(World.w, 0.0001f);
}

float4 ComputeLambertLighting(float4 BaseColor, float3 Normal)
{
    float Diffuse = ComputeLambertTerm(Normal);
    float3 LightColor = GetMainLightColor();
    float3 LitColor = BaseColor.rgb * (0.2f + Diffuse * LightColor);
    return float4(LitColor, BaseColor.a);
}

float4 ComputeBlinnPhongLighting(float4 BaseColor, float3 Normal, float4 MaterialParam, float2 UV)
{
    float3 WorldPosition = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDirection = normalize(CameraWorldPos - WorldPosition);
    float3 LightDirection = normalize(-GetMainLightDirection());
    float3 HalfVector = normalize(ViewDirection + LightDirection);

    float Diffuse = ComputeLambertTerm(Normal);
    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);
    float Specular = pow(saturate(dot(normalize(Normal), HalfVector)), Shininess) * SpecularStrength;

    float3 LightColor = GetMainLightColor();
    float3 DiffuseColor = BaseColor.rgb * (0.2f + Diffuse * LightColor);
    float3 SpecularColor = LightColor * Specular;

    return float4(DiffuseColor + SpecularColor, BaseColor.a);
}
#endif