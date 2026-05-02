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
    : BoxExtent(1.0f, 1.0f, 1.0f),
      BoxCollision(FOBB(FVector(0.0f, 0.0f, 0.0f), FVector(0.5f, 0.5f, 0.5f), FRotator(0.0f, 0.0f, 0.0f)))
{
}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxCollision;
    Ar << BoxExtent;
}

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "BoxExtent", EPropertyType::Vec3, &BoxExtent, 0.f, 100.0f, 0.1f });
}

void UBoxComponent::PostEditProperty(const char* PropertyName)
{
    BoxExtent.X = std::max(0.0f, BoxExtent.X);
    BoxExtent.Y = std::max(0.0f, BoxExtent.Y);
    BoxExtent.Z = std::max(0.0f, BoxExtent.Z);
    UShapeComponent::PostEditProperty(PropertyName);
}

void UBoxComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
    if (bGenerateOverlapEvents)
        return;
}

void UBoxComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    FMatrix WorldMat = GetWorldMatrix();

    WorldMat.M[0][0] = BoxExtent.X;
    WorldMat.M[1][1] = BoxExtent.Y;
    WorldMat.M[2][2] = BoxExtent.Z;

    BoxCollision.Bounds.UpdateAsOBB(WorldMat);

    // (선택) 4. 만약 나중에 BVH나 Octree를 쓴다면 매니저에게 위치가 변했다고 알려줍니다.
    // EnsureWorldAABBUpdated();
    NotifyColliderBVHChanged(this);
}
