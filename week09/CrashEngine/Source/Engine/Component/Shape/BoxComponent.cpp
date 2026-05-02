#include "BoxComponent.h"
#include "Serialization/Archive.h"

#include "GameFrameWork/World.h"

IMPLEMENT_COMPONENT_CLASS(UBoxComponent, UShapeComponent, EEditorComponentCategory::Shapes)


static void NotifyColliderBVHChanged(UShapeComponent* Comp)
{
    if (!Comp)
        return;

    AActor* OwnerActor = Comp->GetOwner();
    if (!OwnerActor)
        return;

    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateCollisionInBVH(Comp);
}

UBoxComponent::UBoxComponent()
    : BoxExtent(0.5f, 0.5f, 0.5f), 
      BoxCollision(FOBB(FVector(0.0f, 0.0f, 0.0f), FVector(0.5f, 0.5f, 0.5f), FRotator(0.0f, 0.0f, 0.0f)))
{

}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxCollision;
    Ar << BoxExtent;
}

void UBoxComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    const FMatrix& WorldMat = GetWorldMatrix();

    BoxCollision.Bounds.UpdateAsOBB(WorldMat);

    // (선택) 4. 만약 나중에 BVH나 Octree를 쓴다면 매니저에게 위치가 변했다고 알려줍니다.
    // EnsureWorldAABBUpdated();
    NotifyColliderBVHChanged(this);
}
