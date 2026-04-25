#pragma once
#include "LightComponentBase.h"
#include "Render/Common/ShadowTypes.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	ULightComponent() = default;

	FMatrix GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj) const;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

public:
	const EShadowMap	 GetShadowMapType() const { return eShadowMapType; };
	void				 SetShadowMapType(EShadowMap eType) { eShadowMapType = eType; }

private:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj) const;
	FMatrix ComputeBasicShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj) const;

protected:
	~ULightComponent() = default;

public:
	float ShadowResolutionScale = 1024.0f;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowSharpen = 0.5f;

private:
	EShadowMap eShadowMapType = { EShadowMap::BASIC } ;
};