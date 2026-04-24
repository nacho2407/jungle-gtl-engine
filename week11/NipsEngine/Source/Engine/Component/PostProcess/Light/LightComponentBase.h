#pragma once
#include "Component/SceneComponent.h"

class ULightComponentBase : public USceneComponent {
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)
	ULightComponentBase() = default;
    virtual void PostDuplicate(UObject* Original) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void Serialize(FArchive& Ar) override;

protected:
	~ULightComponentBase() = default;

public:
    FColor LightColor = FColor::White();
	float Intensity = 1.0f;

	bool bCastShadows = true;
};