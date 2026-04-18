#pragma once

#include "Component/SceneComponent.h"

// Let there be light!
class ULightComponentBase : public USceneComponent {
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

	ULightComponentBase() = default;
    ~ULightComponentBase() = default;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;

public:
	FColor LightColor = FColor::White();
	float Intensity = 1.0f;
	bool bVisible = true;
};
