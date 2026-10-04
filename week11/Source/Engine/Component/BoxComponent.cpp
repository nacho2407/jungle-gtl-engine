#include "BoxComponent.h" 
#include "Object/Object.h"


DEFINE_CLASS(UBoxComponent, UShapeComponent)
REGISTER_FACTORY(UBoxComponent)

void UBoxComponent::PostDuplicate(UObject* Original)
{
    UShapeComponent::PostDuplicate(Original);
	
	UBoxComponent* BoxComp = Cast<UBoxComponent>(Original);
    Extent = BoxComp->Extent;
}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << "Extent" << Extent;
}

bool UBoxComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}

EPrimitiveType UBoxComponent::GetPrimitiveType() const
{
    return EPrimitiveType::EPT_Box;
}
