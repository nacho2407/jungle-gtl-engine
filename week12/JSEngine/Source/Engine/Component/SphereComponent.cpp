#include "SphereComponent.h"
#include "Object/Object.h"



void USphereComponent::PostDuplicate(UObject* Original)
{
	UShapeComponent::PostDuplicate(Original);

	USphereComponent* SphereComp = Cast<USphereComponent>(Original);
	SphereRadius = SphereComp->SphereRadius;
}


void USphereComponent::UpdateWorldAABB() const
{
	const FVector Center = GetWorldLocation();

	const float ScaledRadius = GetScaledSphereRadius();
	WorldAABB.Min = Center - FVector(ScaledRadius, ScaledRadius, ScaledRadius);
	WorldAABB.Max = Center + FVector(ScaledRadius, ScaledRadius, ScaledRadius);
}

bool USphereComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

EPrimitiveType USphereComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Sphere;
}
