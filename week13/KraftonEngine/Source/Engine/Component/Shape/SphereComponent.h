// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Component/ShapeComponent.h"


#include "Source/Engine/Component/Shape/SphereComponent.generated.h"

UCLASS()
class USphereComponent : public UShapeComponent
{
public:
	GENERATED_BODY()
	UFUNCTION(Callable, Category="Shape")
	void SetSphereRadius(float InRadius);
	UFUNCTION(Pure, Category="Shape")
	float GetScaledSphereRadius() const;
	UFUNCTION(Pure, Category="Shape")
	float GetUnscaledSphereRadius() const { return SphereRadius; }

	void ContributeSelectedVisuals(FScene& Scene) const override;
	void UpdateWorldAABB() const override;
	void PostEditProperty(const char* PropertyName) override;
protected:
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Sphere Radius", Min=0.01f, Max=10000.0f, Speed=1.0f)
	float SphereRadius = 2.0f;
};
