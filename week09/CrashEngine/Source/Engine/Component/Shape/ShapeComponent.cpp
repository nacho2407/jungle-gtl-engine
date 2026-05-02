#include "ShapeComponent.h"
#include "Serialization/Archive.h"
#include "GameFrameWork/World.h"
#include "Physics/CollisionManager.h"

IMPLEMENT_CLASS(UShapeComponent, UPrimitiveComponent)


UShapeComponent::UShapeComponent()
{

}
UShapeComponent::~UShapeComponent()
{
    if (UWorld* World = GetWorld()) // (주의: GetWorld() 등 엔진 구조에 맞게 월드를 가져오세요)
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
