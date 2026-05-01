// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PrimitiveComponent.h"
#include "Core/EngineTypes.h"

class UShapeComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

	UShapeComponent();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

	bool SupportsOutline() const override { return false; }
	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }

protected:
	FColor GetShapeColor() const
	{
		return FColor(
			static_cast<uint32>(ShapeColor.X * 255.0f),
			static_cast<uint32>(ShapeColor.Y * 255.0f),
			static_cast<uint32>(ShapeColor.Z * 255.0f),
			static_cast<uint32>(ShapeColor.W * 255.0f)
		);
	}

	FVector4 ShapeColor = { 0.0f, 1.0f, 0.0f, 1.0f }; // Green
	bool bDrawOnlyIfSelected = true;
};
