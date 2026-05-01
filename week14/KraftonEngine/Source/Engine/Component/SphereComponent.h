// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(USphereComponent, UShapeComponent)

	void SetSphereRadius(float InRadius);
	float GetScaledSphereRadius() const;
	float GetUnscaledSphereRadius() const { return SphereRadius; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	void UpdateWorldAABB() const override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	float SphereRadius = 50.0f;
};
