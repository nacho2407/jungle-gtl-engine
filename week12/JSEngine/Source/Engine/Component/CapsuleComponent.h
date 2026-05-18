#pragma once
#include "ShapeComponent.h"

UCLASS(SpawnableComponent, DisplayName = "Capsule Component", Category = "Collision")
class UCapsuleComponent : public UShapeComponent
{
public:
	GENERATED_BODY(UCapsuleComponent, UShapeComponent)
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	float GetCapsuleRadius() const { return CapsuleRadius; }

	void UpdateWorldAABB() const override;

	float GetScaledCapsuleHalfHeight() const 
	{
		FVector Scale = GetWorldScale();
		return CapsuleHalfHeight * std::abs(Scale.Z);
	}
	
	float GetScaledCapsuleRadius() const
	{
		FVector Scale = GetWorldScale();
		return CapsuleRadius * std::abs(Scale.Z);
	}

private:
	UPROPERTY(DisplayName = "Capsule Half Height")
	float CapsuleHalfHeight = 0.5f;

	UPROPERTY(DisplayName = "Capsule Radius")
	float CapsuleRadius = 0.5f;

	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
