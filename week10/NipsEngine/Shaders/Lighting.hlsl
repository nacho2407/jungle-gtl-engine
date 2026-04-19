struct FLightInfo
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
StructuredBuffer<FLightInfo> Lights            : register(t4);
#ifndef CS_SHADER
StructuredBuffer<uint>       CulledIndexBuffer : register(t5);
StructuredBuffer<uint2>      TileBuffer        : register(t6); // .x = offset, .y = count
#endif

struct FAmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float Padding0;
    float3 Color;
    float Intensity;
};

cbuffer FLightConstants : register(b3)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
};

struct LightResult
{
    float3 Diffuse;
    float3 Specular;
    float3 Ambient;
};

// Returns N dot L, normalized
float LambertNdotL(float3 normal, float3 worldPos, float3 lightPos)
{
    float3 N = normalize(normal);
    float3 L = normalize(lightPos - worldPos);
    float NdotL = saturate(dot(N, L));

    return NdotL;
}

LightResult EvaluateAmbientLight(float3 color, float intensity)
{
    LightResult output = (LightResult) 0;
    output.Ambient = color * intensity;
    return output;
}

LightResult EvaluateDirectionalLambert(float3 LightColor, float Intensity, float3 LightDir, float3 Normal)
{
    LightResult output = (LightResult) 0;
    float3 N = normalize(Normal);
    float3 D = normalize(LightDir);
    float NdotD = saturate(dot(N, -D));
    output.Diffuse = LightColor * Intensity * NdotD;
    return output;
}

LightResult EvaluateDirectionalGouraud(
    float3 LightColor,
    float Intensity,
    float3 LightDir,
    float3 Normal,
    float3 SurfaceToCamera,
    float Shininess)
{
    LightResult output = (LightResult) 0;

    float3 N = normalize(Normal);
    float3 L = normalize(-LightDir);
    float3 V = normalize(SurfaceToCamera);

    float NdotL = saturate(dot(N, L));

    // Diffuse
    output.Diffuse = LightColor * Intensity * NdotL;

    // Specular (Blinn-Phong)
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));

    float Spec = NdotL * pow(NdotH, max(Shininess, 1.0));

    output.Specular = LightColor * Intensity * Spec;

    return output;
}

LightResult EvaluateDirectionalBlinnPhong(
    float3 LightColor,
    float Intensity,
    float3 LightDir,
    float3 Normal,
    float3 SurfaceToCamera,
    float Shininess)
{
    // The functions are identical, except that they are called in different context. 
    return EvaluateDirectionalGouraud(LightColor, Intensity, LightDir, Normal, SurfaceToCamera, Shininess);
}

LightResult EvaluatePointLambert(float3 LightColor, float Intensity, float3 Normal, float3 LightPos, float3 WorldPos, float Radius, float Falloff)
{
    LightResult output = (LightResult) 0;
    float3 N = normalize(Normal);
    float3 L = normalize(LightPos - WorldPos);
    float NdotL = saturate(dot(N, L));
    float attenuation = saturate(1.0 - distance(WorldPos, LightPos) / Radius);
    attenuation = pow(attenuation, Falloff);

    output.Diffuse = LightColor * Intensity * attenuation * NdotL;
    return output;
}

LightResult EvaluatePointGouraud(
    float3 LightColor,
    float Intensity,
    float3 LightPos,
    float3 WorldPos,
    float3 Normal,
    float Radius,
    float Falloff,
    float3 SurfaceToCamera,
    float Shininess)
{
    LightResult output = (LightResult) 0;

    float3 N = normalize(Normal);
    float3 L = normalize(LightPos - WorldPos);
    float3 V = normalize(SurfaceToCamera);

    float NdotL = saturate(dot(N, L));
    
    float attenuation = saturate(1.0 - distance(WorldPos, LightPos) / Radius);
    attenuation = pow(attenuation, Falloff);

    // Diffuse
    output.Diffuse = LightColor * Intensity * NdotL * attenuation;

    // Specular (Blinn-Phong)
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));

    float Spec = NdotL * pow(NdotH, max(Shininess, 1.0));

    output.Specular = LightColor * Intensity * Spec * attenuation;

    return output;
}

LightResult EvaluatePointBlinnPhong(
    float3 LightColor,
    float Intensity,
    float3 LightPos,
    float3 WorldPos,
    float3 Normal,
    float Radius,
    float Falloff,
    float3 SurfaceToCamera,
    float Shininess)
{
    return EvaluatePointGouraud(LightColor, Intensity, LightPos, WorldPos, Normal, Radius, Falloff, SurfaceToCamera, Shininess);
}

LightResult EvaluateSpotlightLambert(float3 LightColor,
                                     float Intensity,
                                     float3 Normal,
                                     float3 LightPos,
                                     float3 WorldPos,
                                     float Radius,
                                     float Falloff,
                                     float3 Direction,
                                     float InnerAngle,
                                     float OuterAngle)
{
    LightResult output = (LightResult) 0;
    float3 LightToFrag = normalize(WorldPos - LightPos);
    float theta = acos(dot(LightToFrag, normalize(Direction)));
    
    if (theta > OuterAngle)
    {
        return output;
    }
    
    float epsilon = InnerAngle - OuterAngle;
    float spotAttenuation = saturate((theta - OuterAngle) / epsilon);

    output = EvaluatePointLambert(LightColor, Intensity, Normal, LightPos, WorldPos, Radius, Falloff);
    output.Diffuse *= spotAttenuation;

    return output;
}

LightResult EvaluateSpotlightGouraud(float3 LightColor,
                                     float Intensity,
                                     float3 Normal,
                                     float3 LightPos,
                                     float3 WorldPos,
                                     float Radius,
                                     float Falloff,
                                     float3 Direction,
                                     float InnerAngle,
                                     float OuterAngle,
                                     float3 SurfaceToCamera,
                                     float Shininess)
{
    LightResult output = (LightResult) 0;
    float3 LightToFrag = normalize(WorldPos - LightPos);
    float theta = acos(dot(LightToFrag, normalize(Direction)));
    
    if (theta > OuterAngle)
    {
        return output;
    }
    
    float epsilon = InnerAngle - OuterAngle;
    float spotAttenuation = saturate((theta - OuterAngle) / epsilon);

    output = EvaluatePointGouraud(LightColor, Intensity, LightPos, WorldPos, Normal, Radius, Falloff, SurfaceToCamera, Shininess);
    output.Diffuse   *= spotAttenuation;
    output.Specular  *= spotAttenuation;

    return output;
}


// TODO: Fix
LightResult EvaluateSpotlightBlinnPhong(float3 LightColor,
                                     float Intensity,
                                     float3 Normal,
                                     float3 LightPos,
                                     float3 WorldPos,
                                     float Radius,
                                     float Falloff,
                                     float3 Direction,
                                     float InnerAngle,
                                     float OuterAngle,
                                     float3 SurfaceToCamera,
                                     float Shininess)
{
    LightResult output = (LightResult) 0;
    float3 LightToFrag = normalize(WorldPos - LightPos);
    float theta = acos(dot(LightToFrag, normalize(Direction)));
    
    if (theta > OuterAngle)
    {
        return output;
    }
    
    float epsilon = InnerAngle - OuterAngle;
    float spotAttenuation = saturate((theta - OuterAngle) / epsilon);

    output = EvaluatePointBlinnPhong(LightColor, Intensity, LightPos, WorldPos, Normal, Radius, Falloff, SurfaceToCamera, Shininess);
    output.Diffuse  *= spotAttenuation;
    output.Specular *= spotAttenuation;

    return output;
}