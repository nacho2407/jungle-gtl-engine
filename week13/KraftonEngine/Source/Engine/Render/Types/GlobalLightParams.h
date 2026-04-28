#pragma once
#include "Render/Types/ForwardLightData.h"
struct LightBaseParams
{
	float Intensity; //4
	FVector4 LightColor; //16
	bool bVisible; // 4
	bool bCastShadows = true;

	// Per-light shadow parameters (FShadowSettings override 시 무시됨)
	float ShadowBias = 0.005f;
	float ShadowSlopeBias = 0.005f;
	float ShadowNormalBias = 0.005f;
	float ShadowSharpen = 0.67f;
};
struct FGlobalAmbientLightParams : public LightBaseParams
{

};

struct FGlobalDirectionalLightParams : public LightBaseParams
{
	FVector Direction;
	float ShadowResolutionScale = 1.0f;
};

enum class ECubeMapOrientation {
	CMO_X,
	CMO_negX,
	CMO_Y,
	CMO_negY,
	CMO_Z,
	CMO_negZ,
	CMO_Unknown,
};
struct FPointLightParams : public LightBaseParams
{
	FVector Position;
	float AttenuationRadius;
	float LightFalloffExponent;
	float ShadowResolutionScale = 1;
	uint32 LightType;
	ECubeMapOrientation CubeMapOrientation = ECubeMapOrientation::CMO_Unknown;

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
		Info.bCastShadow = bCastShadows ? 1u : 0u;
		Info.ShadowMapIndex = 0;
		Info.ShadowAtlasScaleBias = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
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