#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resources/Managers/MeshBufferManager.h"
#include "Core/CollisionTypes.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"

#include <cmath>
#include <cstring>

namespace
{
bool HasSameTransformBasis(const FMatrix& A, const FMatrix& B)
{
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Col = 0; Col < 3; ++Col)
        {
            if (A.M[Row][Col] != B.M[Row][Col])
            {
                return false;
            }
        }
    }

    return true;
}
} // namespace

IMPLEMENT_CLASS(UPrimitiveComponent, USceneComponent)

UPrimitiveComponent::~UPrimitiveComponent()
{
    DestroyRenderState();
}

void UPrimitiveComponent::MarkProxyDirty(EDirtyFlag Flag) const
{
    if (!SceneProxy || !Owner || !Owner->GetWorld())
        return;
    Owner->GetWorld()->GetScene().MarkProxyDirty(SceneProxy, Flag);
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << bIsVisible;
    // LocalExtents는 메시 등에서 재계산되므로 직렬화 제외.
}

void UPrimitiveComponent::SetVisibility(bool bNewVisible)
{
    if (bIsVisible == bNewVisible)
        return;
    bIsVisible = bNewVisible;
    MarkRenderVisibilityDirty();
}

// ============================================================
// MarkRenderTransformDirty / MarkRenderVisibilityDirty
//   프록시 dirty + Octree(액터 단위 dirty) + PickingBVH dirty
//   호출자가 외워야 했던 시퀀스를 단일 진입점으로 통합.
// ============================================================
void UPrimitiveComponent::MarkRenderTransformDirty()
{
    MarkProxyDirty(EDirtyFlag::Transform);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateActorInOctree(OwnerActor);
    World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::MarkRenderVisibilityDirty()
{
    MarkProxyDirty(EDirtyFlag::Visibility);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    // 가시성 변화는 Octree 포함 여부도 좌우하므로 액터 dirty로 반영한다.
    World->UpdateActorInOctree(OwnerActor);
    World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
    // 베이스 클래스의 transform 등 공통 프로퍼티 처리 보장
    USceneComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Visible") == 0)
    {
        // Property Editor가 bIsVisible을 직접 수정한 경우 dirty 시퀀스만 전파한다.
        MarkRenderVisibilityDirty();
    }
}

FBoundingBox UPrimitiveComponent::GetWorldBoundingBox() const
{
    EnsureWorldAABBUpdated();
    return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
}

void UPrimitiveComponent::MarkWorldBoundsDirty()
{
    // Local bounds(shape) 자체가 바뀐 경우용 진입점.
    // fast-path(이전 AABB를 translation만으로 재사용)는 shape가 동일하다는 가정에 의존하므로
    // 여기서는 반드시 무력화해야 한다. 안 그러면 mesh 교체 후에도 stale AABB가 캐시된다.
    bWorldAABBDirty = true;
    bHasValidWorldAABB = false;
    MarkRenderTransformDirty();
}

void UPrimitiveComponent::UpdateWorldAABB() const
{
    FVector LExt = LocalExtents;

    FMatrix worldMatrix = GetWorldMatrix();

    float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
    float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
    float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

    FVector WorldCenter = GetWorldLocation();
    WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
    WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

/* 현재 쓰이지 않는 코드입니다*/
bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    FMeshDataView View = GetMeshDataView();
    if (!View.IsValid())
        return false;

    bool bHit = FRayUtils::RaycastTriangles(
        Ray, GetWorldMatrix(),
        GetWorldInverseMatrix(),
        View.VertexData,
        View.Stride,
        View.IndexData,
        View.IndexCount,
        OutHitResult);

    if (bHit)
    {
        OutHitResult.HitComponent = this;
    }
    return bHit;
}

void UPrimitiveComponent::UpdateWorldMatrix() const
{
    const FMatrix PreviousWorldMatrix = CachedWorldMatrix;
    const FVector PreviousWorldAABBMin = WorldAABBMinLocation;
    const FVector PreviousWorldAABBMax = WorldAABBMaxLocation;
    const bool bHadValidWorldAABB = bHasValidWorldAABB;

    USceneComponent::UpdateWorldMatrix();

    if (bWorldAABBDirty)
    {
        if (bHadValidWorldAABB && HasSameTransformBasis(PreviousWorldMatrix, CachedWorldMatrix))
        {
            const FVector TranslationDelta = CachedWorldMatrix.GetLocation() - PreviousWorldMatrix.GetLocation();
            WorldAABBMinLocation = PreviousWorldAABBMin + TranslationDelta;
            WorldAABBMaxLocation = PreviousWorldAABBMax + TranslationDelta;
            bWorldAABBDirty = false;
            bHasValidWorldAABB = true;
        }
        else
        {
            UpdateWorldAABB();
        }
    }

    // 프록시가 등록된 경우 Transform dirty 전파 (FScene DirtySet에도 등록)
    MarkProxyDirty(EDirtyFlag::Transform);
}

// --- 프록시 팩토리 ---
FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
    // 기본 PrimitiveComponent용 프록시
    return new FPrimitiveSceneProxy(this);
}

// --- 렌더 상태 관리 (UE RegisterComponent 대응) ---
void UPrimitiveComponent::CreateRenderState()
{
    if (SceneProxy)
        return; // 이미 등록됨

    // Owner → World → FScene 경로로 접근
    if (!Owner || !Owner->GetWorld())
        return;
    FScene& Scene = Owner->GetWorld()->GetScene();
    SceneProxy = Scene.AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
    // SceneProxy가 없더라도 Octree에는 등록돼 있을 수 있으므로 partition 정리는 항상 시도한다.
    if (Owner)
    {
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetPartition().RemoveSinglePrimitive(this);
            World->MarkWorldPrimitivePickingBVHDirty();

            if (SceneProxy)
            {
                // Scene.RemovePrimitive 가 VisibleProxies 캐시도 일관되게 정리한다.
                World->GetScene().RemovePrimitive(SceneProxy);
            }
        }
    }
    SceneProxy = nullptr;
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
    // 프록시 파괴 후 재생성 — 메시 교체 등 큰 변경 시 사용
    DestroyRenderState();
    CreateRenderState();
}


void UPrimitiveComponent::OnTransformDirty()
{
    // 순수 transform 변경 — local bounds(shape)는 그대로이므로 fast-path를 살린다.
    // (basis 동일 + translation만 바뀐 경우 UpdateWorldMatrix가 이전 AABB를 평행이동만 적용)
    bWorldAABBDirty = true;
    MarkRenderTransformDirty();
}

void UPrimitiveComponent::EnsureWorldAABBUpdated() const
{
    GetWorldMatrix();
    if (bWorldAABBDirty)
    {
        UpdateWorldAABB();
    }
}
