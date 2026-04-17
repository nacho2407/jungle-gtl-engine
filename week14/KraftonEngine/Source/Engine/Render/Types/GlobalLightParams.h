#pragma once
#include "Render/Pipeline/ForwardLightData.h"
struct LightBaseParams
{
	float Intensity; //4
	FVector4 LightColor; //16 
	bool bVisible; // 4
};
struct FGlobalAmbientLightParams : public LightBaseParams
{

};

struct FGlobalDirectionalLightParams : public LightBaseParams
{
	FVector Direction;
};

struct FPointLightParams : public LightBaseParams
{
	FVector Position;
	float AttenuationRadius;
	float LightFalloffExponent;
	uint32 LightType;

	virtual FLightInfo ToLightInfo() const
	{
		FLightInfo Info;
		Info.Position = Position;
		Info.AttenuationRadius = AttenuationRadius;

		Info.Color = LightColor;
		Info.Intensity = Intensity;

		Info.Direction = FVector(0.f, 0.f, 0.f);
		Info.FalloffExponent = LightFalloffExponent;

		Info.InnerConeCos = 0.f;
		Info.OuterConeCos = 0.f;
		Info.LightType = LightType;
		return Info;
	}
};

struct FSpotLightParams : public FPointLightParams
{
	FVector Direction;
	float InnerConeCos;
	float OuterConeCos;

	virtual FLightInfo ToLightInfo() const override
	{
		FLightInfo Info = FPointLightParams::ToLightInfo();
		Info.Direction = Direction;
		Info.InnerConeCos = InnerConeCos;
		Info.OuterConeCos = OuterConeCos;
		return Info;
	}

};