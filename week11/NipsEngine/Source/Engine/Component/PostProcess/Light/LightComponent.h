#pragma once
#include "LightComponentBase.h"
#include "Render/Common/ShadowTypes.h"
#include "Core/EngineTypes.h"

class UMaterialInterface;

class ULightComponent : public ULightComponentBase {
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	ULightComponent() = default;

	FMatrix GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds = nullptr) const;
	
	/* Cascade ShadowMap 전용 */
	FMatrix GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT, const TArray<FBoundingBox>* VisibleObjectsBounds = nullptr) const;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

public:
	EShadowMap GetShadowMapType() const { return eShadowMapType; }
	void SetShadowMapType(EShadowMap InType) { eShadowMapType = InType; }

private:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const;

	FMatrix ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT) const;
protected:
	~ULightComponent() = default;

public:
	float ShadowResolutionScale = 1024.0f;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowSharpen = 0.5f;

private:
	EShadowMap eShadowMapType = EShadowMap::CSM;
};
