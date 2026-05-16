#pragma once
#include "ShapeComponent.h"

UCLASS()
class USphereComponent : public UShapeComponent
{
public:
	DECLARE_CLASS(USphereComponent, UShapeComponent)
	float GetSphereRadius() const { return SphereRadius; }
	float GetScaledSphereRadius() const
	{
		return SphereRadius;
	}

	void PostDuplicate(UObject* Original) override;

private:
	UPROPERTY(DisplayName = "Sphere Radius")
	float SphereRadius = 0.5f;

	// UShapeComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
