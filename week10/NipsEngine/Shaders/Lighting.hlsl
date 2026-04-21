struct LightInfo
{
    float3 Color;
    float Intensity;

    uint Type;
    float Radius;
    float InnerAngle;
    float OuterAngle;

    float3 Direction;
    float Falloff;

    float3 Position;
    float Padding1;
};

struct AmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct DirectionalLightInfo
{
    float3 Direction;
    float Padding0;
    float3 Color;
    float Intensity;
};

cbuffer LightConstants : register(b3)
{
    AmbientLightInfo AmbientLight;
    DirectionalLightInfo DirectionalLight;
    uint LightCount;
    uint DecalCount;
    float2 Padding;
};

StructuredBuffer<LightInfo> Lights : register(t4);
#ifndef CS_SHADER
StructuredBuffer<uint> CulledIndexBuffer : register(t5);
StructuredBuffer<uint2> TileBuffer : register(t6); // .x = offset, .y = count
#endif

float3 CalcAmbient(AmbientLightInfo light, float3 albedo)
{
    return light.Color * light.Intensity * albedo;
}

float3 CalcDirectionalLambert(DirectionalLightInfo light, float3 albedo, float3 normal)
{
    float3 N = normalize(normal);
    float3 L = normalize(-light.Direction);
    float NdotL = saturate(dot(N, L));
    
    // Diffuse
    float3 diffuse = light.Color * light.Intensity * NdotL * albedo;
    
    return diffuse;
}

float3 CalcDirectionalBlinnPhong(DirectionalLightInfo light, float3 albedo, float3 normal, float3 worldPos, float3 surfaceToCamera, float shininess)
{
    float3 N = normalize(normal);
    float3 L = normalize(-light.Direction);
    float3 V = normalize(surfaceToCamera);
    float NdotL = saturate(dot(N, L));
    
    // Diffuse
    float3 diffuse = light.Color * light.Intensity * NdotL * albedo;
    
    // Specular (Blinn-Phong)
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float Spec = NdotL * pow(NdotH, max(shininess, 1.0));
    float3 specular = light.Color * light.Intensity * Spec;
    
    return diffuse + specular;
}

float3 CalcPointLambert(LightInfo light, float3 albedo, float3 normal, float3 worldPos)
{
    float3 N = normalize(normal);
    float3 L = normalize(light.Position - worldPos);
    float NdotL = saturate(dot(N, L));
    float attenuation = saturate(1.0 - distance(worldPos, light.Position) / light.Radius);
    attenuation = pow(attenuation, light.Falloff);
    // Diffuse
    float3 diffuse = light.Color * light.Intensity * attenuation * NdotL * albedo;
    
    return diffuse;
}

float3 CalcPointBlinnPhong(LightInfo light, float3 albedo, float3 normal, float3 worldPos, float3 surfaceToCamera, float shininess)
{
    float3 N = normalize(normal);
    float3 L = normalize(light.Position - worldPos);
    float3 V = normalize(surfaceToCamera);
    
    float NdotL = saturate(dot(N, L));
    float attenuation = saturate(1.0 - distance(worldPos, light.Position) / light.Radius);
    attenuation = pow(attenuation, light.Falloff);
    
    // Diffuse
    float3 diffuse = light.Color * light.Intensity * NdotL * attenuation;
    
    // Specular (Blinn-Phong)
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float Spec = NdotL * pow(NdotH, max(shininess, 1.0));
    float3 specular = light.Color * light.Intensity * Spec * attenuation;
    
    return diffuse + specular;
}

float3 CalcSpotlightLambert(LightInfo light, float3 albedo, float3 normal, float3 worldPos)
{
    float3 LightToFrag = normalize(worldPos - light.Position);
    float theta = acos(dot(LightToFrag, normalize(light.Direction)));
    
    if (theta > light.OuterAngle)
    {
        return float3(0, 0, 0);
    }
    
    float epsilon = light.InnerAngle - light.OuterAngle;
    float spotAttenuation = saturate((theta - light.OuterAngle) / epsilon);
    return CalcPointLambert(light, albedo, normal, worldPos) * spotAttenuation;
}

float3 CalcSpotlightBlinnPhong(LightInfo light, float3 albedo, float3 normal, float3 worldPos, float3 surfaceToCamera, float shininess)
{
    float3 LightToFrag = normalize(worldPos - light.Position);
    float theta = acos(dot(LightToFrag, normalize(light.Direction)));
    
    if (theta > light.OuterAngle)
    {
        return float3(0, 0, 0);
    }
    
    float epsilon = light.InnerAngle - light.OuterAngle;
    float spotAttenuation = saturate((theta - light.OuterAngle) / epsilon);
    return CalcPointBlinnPhong(light, albedo, normal, worldPos, surfaceToCamera, shininess) * spotAttenuation;
}
