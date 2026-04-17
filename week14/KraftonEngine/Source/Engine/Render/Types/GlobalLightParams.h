#pragma once

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