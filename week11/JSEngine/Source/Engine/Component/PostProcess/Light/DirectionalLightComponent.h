#pragma once
#include "LightComponent.h"

#include "DirectionalLightComponent.generated.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
    GENERATED_BODY_UDirectionalLightComponent()
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void Serialize(FArchive& Ar) override;

protected:
	FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

public:
	UPROPERTY(EditAnywhere, Category="Shadow", DisplayName="CSM Max Distance")
	float CSMMaxDistance = { 300.f };
	UPROPERTY(EditAnywhere, Category="Shadow", DisplayName="CSM Practical Lambda")
	float CSMPractialLambda = { 0.25f };

};
