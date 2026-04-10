#include "HeightFogComponent.h"
#include "Object/Object.h"


DEFINE_CLASS(UHeightFogComponent, UPrimitiveComponent)
REGISTER_FACTORY(UHeightFogComponent)

UHeightFogComponent::UHeightFogComponent() 
{
}

UHeightFogComponent* UHeightFogComponent::Duplicate() 
{
    UHeightFogComponent* NewComp = UObjectManager::Get().CreateObject<UHeightFogComponent>();

	NewComp->FogDensity = FogDensity;
    NewComp->HeightFalloff = HeightFalloff;
    NewComp->FogInscatteringColor = FogInscatteringColor;

	return NewComp;
}

void UHeightFogComponent::UpdateWorldAABB() const 
{
}

bool UHeightFogComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) 
{ 
	return false; 
}
