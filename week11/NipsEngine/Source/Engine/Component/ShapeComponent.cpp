#include "ShapeComponent.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)
REGISTER_FACTORY(UShapeComponent)

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

void UShapeComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << "ShapeColor" << ShapeColor;
    Ar << "DrawOnlyIfSelected" << bDrawOnlyIfSelected;
}
