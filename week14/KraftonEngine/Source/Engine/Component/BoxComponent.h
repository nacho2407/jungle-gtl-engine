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
	// UpdateWorldAABB는 base UPrimitiveComponent의 회전 반영 버전을 그대로 사용.
	// (BoxComponent::SetBoxExtent가 LocalExtents = BoxExtent로 동기화)
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	FVector BoxExtent = { 1.0f, 1.0f, 1.0f };
};
