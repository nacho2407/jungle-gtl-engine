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

	void PostDuplicate(UObject* Original) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	
	void Serialize(FArchive& Ar) override;

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
	int32 ShadowResolutionScale = 1024;
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
	float ShadowSharpen = 0.5f;

	// 디버그용으로 Shadow Atlas에서 해당 라이트의 타일 위치와 크기를 저장하는 변수, 현재 지워도됩니다
    FVector4 DebugShadowAtlasScaleOffset;
    bool bHasDebugShadowAtlasTile = false;

private:
	EShadowMap eShadowMapType = EShadowMap::CSM;
};
