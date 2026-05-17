#pragma once
#include "Component/SceneComponent.h"

#include "LightComponentBase.generated.h"

UCLASS()
class ULightComponentBase : public USceneComponent {
    GENERATED_BODY_ULightComponentBase()
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	ULightComponentBase() = default;
    virtual void PostDuplicate(UObject* Original) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void Serialize(FArchive& Ar) override;

protected:
	~ULightComponentBase() = default;

public:
    UPROPERTY(EditAnywhere)
	FColor LightColor = FColor::White();
	UPROPERTY(EditAnywhere)
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere)
	bool bCastShadows = true;
};
