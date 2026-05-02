#include "BoxComponent.h"
#include "Serialization/Archive.h"

#include "GameFrameWork/World.h"

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)


UBoxComponent::UBoxComponent()
    : BoxExtent(0.5f, 0.5f, 0.5f), 
      BoxCollision(FOBB(FVector(0.0f, 0.0f, 0.0f), FVector(0.5f, 0.5f, 0.5f), FRotator(0.0f, 0.0f, 0.0f)))
{

}

void UBoxComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << BoxCollision;
}

void UBoxComponent::OnTransformDirty()
{
    UShapeComponent::OnTransformDirty();

    // (GetWorldMatrix() 내부에서 bTransformDirty가 true면 새로 행렬을 계산할 것입니다)
    const FMatrix& WorldMat = GetWorldMatrix();

    BoxCollision.Bounds.UpdateAsOBB(WorldMat);

	BoxExtent = BoxCollision.Bounds.Extent;
    // (선택) 4. 만약 나중에 BVH나 Octree를 쓴다면 매니저에게 위치가 변했다고 알려줍니다.
    // EnsureWorldAABBUpdated();
}
