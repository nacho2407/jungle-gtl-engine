// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Component/ShapeComponent.h"


#include "Source/Engine/Component/Shape/SphereComponent.generated.h"

UCLASS()
class USphereComponent : public UShapeComponent
{
public:
	GENERATED_BODY()
	void SetSphereRadius(float InRadius);
	float GetScaledSphereRadius() const;
	float GetUnscaledSphereRadius() const { return SphereRadius; }

	void UpdateWorldAABB() const override;
	void PostEditProperty(const char* PropertyName) override;
protected:
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Sphere Radius", Min=0.01f, Max=10000.0f, Speed=1.0f)
	float SphereRadius = 2.0f;
};
