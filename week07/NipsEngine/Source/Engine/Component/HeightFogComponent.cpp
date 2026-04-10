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
    /** 아예 Bounding Box 가 없으면 Collect 자체가 안돼서 임의로 지정 */
    FAABB AABB(FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f));
    WorldAABB = AABB;
}

bool UHeightFogComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) 
{ 
	return false; 
}
