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
