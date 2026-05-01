// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

	void SetCapsuleSize(float InRadius, float InHalfHeight);
	float GetScaledCapsuleRadius() const;
	float GetScaledCapsuleHalfHeight() const;
	float GetUnscaledCapsuleRadius() const { return CapsuleRadius; }
	float GetUnscaledCapsuleHalfHeight() const { return CapsuleHalfHeight; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	void UpdateWorldAABB() const override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	float CapsuleRadius = 22.0f;
	float CapsuleHalfHeight = 44.0f;
};
