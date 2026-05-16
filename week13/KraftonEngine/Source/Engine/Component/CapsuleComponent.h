// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShapeComponent.h"


#include "Source/Engine/Component/CapsuleComponent.generated.h"

UCLASS()
class UCapsuleComponent : public UShapeComponent
{
public:
	GENERATED_BODY()
	void SetCapsuleSize(float InRadius, float InHalfHeight);
	float GetScaledCapsuleRadius() const;
	float GetScaledCapsuleHalfHeight() const;
	float GetUnscaledCapsuleRadius() const { return CapsuleRadius; }
	float GetUnscaledCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	void UpdateWorldAABB() const override;
	void PostEditProperty(const char* PropertyName) override;

protected:
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Capsule Radius", Min=0.01f, Max=10000.0f, Speed=1.0f)
	float CapsuleRadius = 1.8f;
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Capsule Half Height", Min=0.01f, Max=10000.0f, Speed=1.0f)
	float CapsuleHalfHeight = 3.0f;
};
