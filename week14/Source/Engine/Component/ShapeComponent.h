// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Component/PrimitiveComponent.h"
#include "Core/Types/EngineTypes.h"


#include "Source/Engine/Component/ShapeComponent.generated.h"

UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UShapeComponent();

	void PostEditProperty(const char* PropertyName) override;
	bool SupportsOutline() const override { return false; }
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	bool IsDrawOnlyIfSelected() const { return bDrawOnlyIfSelected; }
	const FVector4& GetShapeColorVec4() const { return ShapeColor; }

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

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Shape Color", Type=Color4)
	FVector4 ShapeColor = { 0.0f, 1.0f, 0.0f, 1.0f }; // Green
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Draw Only If Selected")
	bool bDrawOnlyIfSelected = false;
};
