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
static const int DOFPoissonSampleCount = 32;
static const float2 DOFPoissonSamples[32] =
{
    float2( 0.0000f,  0.0000f),
    float2( 0.1792f, -0.1148f),
    float2(-0.2070f,  0.1329f),
    float2( 0.1034f,  0.3018f),
    float2(-0.3476f, -0.1804f),
    float2( 0.4015f,  0.0889f),
    float2(-0.0522f, -0.4538f),
    float2(-0.4833f,  0.3022f),
    float2( 0.5320f, -0.3032f),
    float2( 0.2956f,  0.5341f),
    float2(-0.6424f, -0.0486f),
    float2( 0.0559f, -0.6743f),
    float2(-0.3071f,  0.6929f),
    float2( 0.7420f,  0.3147f),
    float2(-0.7478f, -0.3765f),
    float2( 0.3421f, -0.7893f),
    float2(-0.0263f,  0.8587f),
    float2( 0.8677f, -0.0793f),
    float2(-0.8734f,  0.1320f),
    float2( 0.5156f,  0.7485f),
    float2(-0.5150f, -0.7605f),
    float2( 0.0460f, -0.9373f),
    float2(-0.1885f,  0.9580f),
    float2( 0.9538f,  0.2307f),
    float2(-0.9712f, -0.1722f),
    float2( 0.6814f, -0.7081f),
    float2(-0.6835f,  0.7203f),
    float2( 0.2694f,  0.9624f),
    float2(-0.4058f, -0.9140f),
    float2( 0.9875f, -0.5150f),
    float2(-0.9834f,  0.5144f),
    float2( 0.7621f,  0.6464f)
};

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

float2 MapDiskSampleToPolygonAperture(float2 diskSample, float bladeCount, float polygonStrength)
{
    float radius = length(diskSample);
    if (radius <= 0.0001f)
    {
        return 0.0f;
    }

    float angle = atan2(diskSample.y, diskSample.x);
    float polygonRadius = lerp(1.0f, PolygonBoundaryRadius(angle, bladeCount), saturate(polygonStrength));
    return diskSample * polygonRadius;
}

float DominantSignedCoC(float nearCoC, float farCoC)
{
    return (abs(nearCoC) > farCoC) ? nearCoC : farCoC;
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
