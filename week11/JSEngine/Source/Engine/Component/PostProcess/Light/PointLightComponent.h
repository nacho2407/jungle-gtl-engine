#pragma once
#include "LightComponent.h"

#include "PointLightComponent.generated.h"

UCLASS()
class UPointLightComponent : public ULightComponent
{
    GENERATED_BODY_UPointLightComponent()
public:
    DECLARE_CLASS(UPointLightComponent, ULightComponent)
    virtual void PostDuplicate(UObject* Original) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void Serialize(FArchive& Ar) override;

protected:
	virtual FMatrix ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
		const TArray<FBoundingBox>* VisibleObjectsBounds) const override;

	//virtual void PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const override;

public:
    UPROPERTY(EditAnywhere)
    float AttenuationRadius		= 10.f;
    UPROPERTY(EditAnywhere)
    float LightFalloffExponent	= 1.f;
};
