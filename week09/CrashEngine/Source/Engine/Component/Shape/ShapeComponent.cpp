#include "ShapeComponent.h"
#include "Serialization/Archive.h"
#include "GameFrameWork/World.h"
#include "Physics/CollisionManager.h"

IMPLEMENT_HIDDEN_COMPONENT_CLASS(UShapeComponent, UPrimitiveComponent)


UShapeComponent::UShapeComponent()
{

}
UShapeComponent::~UShapeComponent()
{
    if (UWorld* World = GetWorld())
    {
        if (FCollisionManager* CollisionMgr = World->GetCollisionManager())
        {
            CollisionMgr->UnregisterComponent(this);
        }
    }
}
void UShapeComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << ShapeColor;
    Ar << bDrawOnlyIfSelected;
}

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "ShapeColor", EPropertyType::Color4, &ShapeColor});
    OutProps.push_back({ "AlwaysDrawCollider", EPropertyType::Bool, &bDrawOnlyIfSelected });
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UShapeComponent::PostDuplicate()
{
    UPrimitiveComponent::PostDuplicate();
    OnOwnerChanged();
    MarkWorldBoundsDirty();
}

void UShapeComponent::OnTransformDirty()
{
    UPrimitiveComponent::OnTransformDirty();
}

void UShapeComponent::OnOwnerChanged()
{
    AActor* actor = GetOwner();

	if (!actor)
        return;

    UWorld* world = actor->GetWorld();

	if (!world)
        return;

    world->GetCollisionManager()->RegisterComponent(this);
}
