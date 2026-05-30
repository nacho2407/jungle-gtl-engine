#ifndef DEPTH_OF_FIELD_HLSL
#define DEPTH_OF_FIELD_HLSL

cbuffer DOFConstantBuffer : register(b2)
{
    float4 DOFParams0; // x=FocalDistance, y=Aperture(F-Stop), z=MaxCoCRadius, w=NearClip
    float4 DOFParams1; // x=FarClip, y=InvFullWidth, z=InvFullHeight, w=InvHalfWidth
    float4 DOFParams2; // x=InvHalfHeight, y=ApertureBladeCount, z=BokehThreshold, w=BokehIntensity
    float4 DOFParams3; // x=BokehRadiusScale
};

#define DOFFocalDistance DOFParams0.x
#define DOFAperture DOFParams0.y
#define DOFMaxCoCRadius DOFParams0.z
#define DOFNearClip DOFParams0.w
#define DOFFarClip DOFParams1.x
#define DOFInvFullResolution DOFParams1.yz
#define DOFInvHalfResolution float2(DOFParams1.w, DOFParams2.x)
#define DOFApertureBladeCount DOFParams2.y
#define DOFBokehThreshold DOFParams2.z
#define DOFBokehIntensity DOFParams2.w
#define DOFBokehRadiusScale DOFParams3.x

static const float DOFTwoPi = 6.28318530f;

float InterleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float PolygonBoundaryRadius(float angle, float bladeCount)
{
    bladeCount = clamp(bladeCount, 3.0f, 16.0f);
    float sectorAngle = DOFTwoPi / bladeCount;
    float localAngle = frac((angle + sectorAngle * 0.5f) / sectorAngle) * sectorAngle - sectorAngle * 0.5f;
    return cos(sectorAngle * 0.5f) / max(cos(localAngle), 0.001f);
}

float BokehHighlightRatio(float3 color)
{
    float threshold = max(DOFBokehThreshold, 0.0f);
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    return saturate((luminance - threshold) / max(luminance, 0.001f));
}

float LinearizeSceneDepth(float Depth)
{
    return DOFNearClip * DOFFarClip / (DOFNearClip - Depth * (DOFNearClip - DOFFarClip));
}

float CalculateSignedCoC(float Depth)
{
    float ViewDepth = LinearizeSceneDepth(Depth);
    float FocusDistance = max(DOFFocalDistance, DOFNearClip);
    float SignedDistance = ViewDepth - FocusDistance;
    float FocusRange = FocusDistance * max(DOFAperture, 0.01f);
    float Radius = saturate(abs(SignedDistance) / max(FocusRange, 0.001f)) * DOFMaxCoCRadius;
    return (SignedDistance < 0.0f) ? -Radius : Radius;
}

#endif
