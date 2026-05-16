// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	static void RegisterProperties(UClass* Class);

	void SetBoxExtent(const FVector& InExtent);
	FVector GetScaledBoxExtent() const;
	FVector GetUnscaledBoxExtent() const { return BoxExtent; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	// UpdateWorldAABB는 base UPrimitiveComponent의 회전 반영 버전을 그대로 사용.
	// (BoxComponent::SetBoxExtent가 LocalExtents = BoxExtent로 동기화)
	void PostEditProperty(const char* PropertyName) override;

protected:
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Box Extent", Type=Vec3, Min=0.01f, Max=0.0f, Speed=0.1f)
	FVector BoxExtent = { 1.0f, 1.0f, 1.0f };
};
