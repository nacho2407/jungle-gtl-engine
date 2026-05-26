#pragma once
#include "PrimitiveComponent.h"

UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UShapeComponent, UPrimitiveComponent)

	void PostDuplicate(UObject* Original) override;

	virtual bool LineTraceShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionQueryParams& Params) const;

	virtual bool SweepShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params) const;

private:
	UPROPERTY(DisplayName = "Shape Color")
	FColor ShapeColor;

	UPROPERTY(DisplayName = "Draw Only If Selected")
	bool bDrawOnlyIfSelected;

	// UPrimitiveComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
