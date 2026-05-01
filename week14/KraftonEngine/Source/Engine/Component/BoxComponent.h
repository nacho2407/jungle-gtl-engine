// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

	void SetBoxExtent(const FVector& InExtent);
	FVector GetScaledBoxExtent() const;
	FVector GetUnscaledBoxExtent() const { return BoxExtent; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	void UpdateWorldAABB() const override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	FVector BoxExtent = { 0.5f, 0.5f, 0.5f };
};
