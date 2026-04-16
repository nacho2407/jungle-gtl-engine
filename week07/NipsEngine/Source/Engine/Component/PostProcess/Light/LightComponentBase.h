#pragma once
#include "Component/SceneComponent.h"

class ULightComponentBase : public USceneComponent {
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	ULightComponentBase() = default;
	virtual void PostDuplicate(UObject* Original) override;

	// Accesors
	//FColor	GetLightColor() const { return LightColor; }
	//void	SetLightColor(const FColor& InColor) { LightColor = InColor; }
	//float	GetIntensity() const { return Intensity; }
	//void	SetIntensity(float InIntensity) { Intensity = InIntensity; }
	//float	GetIndirectLightingIntensity() const { return IndirectLightingIntensity; }
	//void	SetIndirectLightingIntensity(float InContribution) { IndirectLightingIntensity = InContribution; }
	//bool	IsAffectingWorld() const { return bAffectsWorld; }
	//void	SetAffectingWorld(bool bInAffectsWorld) { bAffectsWorld = bInAffectsWorld; }


protected:
	~ULightComponentBase() = default;

public:
	FColor	LightColor;
	float	Intensity					= 1.0f;
	float	IndirectLightingIntensity	= 0.f;
	bool	bAffectsWorld				= true;
	bool	bAffectReflection			= true; // Not used for now
};