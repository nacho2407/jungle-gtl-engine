#include "ShapeComponent.h"

bool UShapeComponent::LineTraceShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionQueryParams& Params) const
{
	(void)OutHit;
	(void)StartWS;
	(void)EndWS;
	(void)Params;
	return false;
}

bool UShapeComponent::SweepShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionShape& CollisionShape,
	const FCollisionQueryParams& Params) const
{
	(void)OutHit;
	(void)StartWS;
	(void)EndWS;
	(void)CollisionShape;
	(void)Params;
	return false;
}

void UShapeComponent::UpdateWorldAABB() const
{
}

bool UShapeComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

EPrimitiveType UShapeComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Shape;
}

void UShapeComponent::PostDuplicate(UObject* Original)
{
	UPrimitiveComponent::PostDuplicate(Original);

	UShapeComponent* ShapeComp = Cast<UShapeComponent>(Original);
	ShapeColor = ShapeComp->ShapeColor;
	bDrawOnlyIfSelected = ShapeComp->bDrawOnlyIfSelected;
}
