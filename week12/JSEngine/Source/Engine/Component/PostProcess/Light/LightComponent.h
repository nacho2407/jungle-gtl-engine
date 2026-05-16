#pragma once
#include "LightComponentBase.h"
#include "Render/Common/ShadowTypes.h"
#include "Core/EngineTypes.h"

class UMaterialInterface;

UCLASS()
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
	EShadowMap GetShadowMapType() const { return ShadowMapType; }
	void SetShadowMapType(EShadowMap InType) { ShadowMapType = InType; }

protected:
	virtual FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const { return FMatrix::Identity; }

	virtual FMatrix ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		float SplitNearT, float SplitFarT) const;

	virtual void PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const;

protected:
	~ULightComponent() = default;

public:
	int32 ShadowResolutionScale = 2048;
	float ConstantBias = { 0.003f };
	float SlopeScaledBias = { 0.12f } ;
	float ShadowSharpen = 0.5f;

	// 디버그용으로 Shadow Atlas에서 해당 라이트의 타일 위치와 크기를 저장하는 변수
	FVector4 DebugShadowAtlasScaleOffset;
	bool bHasDebugShadowAtlasTile = false;
	int32 DebugShadowCubeIndex;
	bool bHasDebugShadowCubeTile = false;

protected:
	UPROPERTY(DisplayName = "Shadow Map Type")
	EShadowMap ShadowMapType = EShadowMap::CSM;
};
