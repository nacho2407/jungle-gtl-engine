#pragma once
#include "Component/SceneComponent.h"

UCLASS()
class ULightComponentBase : public USceneComponent {
public:
	GENERATED_BODY(ULightComponentBase, USceneComponent)
	ULightComponentBase() = default;
	virtual void PostDuplicate(UObject* Original) override;
protected:
	~ULightComponentBase() = default;

public:
	UPROPERTY(DisplayName = "Color")
	FColor LightColor = FColor::White();

	UPROPERTY(DisplayName = "Intensity", Speed = 0.1f)
	float Intensity = 1.0f;

	UPROPERTY(DisplayName = "Cast Shadows")
	bool bCastShadows = true;
};
