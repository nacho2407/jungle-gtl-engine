// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Core/CollisionTypes.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
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

IMPLEMENT_HIDDEN_COMPONENT_CLASS(UPrimitiveComponent, USceneComponent)

UPrimitiveComponent::~UPrimitiveComponent()
{
    DestroyRenderState();
}

void UPrimitiveComponent::MarkProxyDirty(ESceneProxyDirtyFlag Flag) const
{
    if (!SceneProxy || !Owner || !Owner->GetWorld())
        return;
    Owner->GetWorld()->GetScene().MarkProxyDirty(SceneProxy, Flag);
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << bIsVisible;
    Ar << bVisibleInEditor;
    Ar << bVisibleInGame;
    Ar << bIsEditorHelper;
}

void UPrimitiveComponent::SetVisibility(bool bNewVisible)
{
    const bool bNeedsChildSync = (bVisibleInEditor != bNewVisible) || (bVisibleInGame != bNewVisible);
    if (bIsVisible == bNewVisible && !bNeedsChildSync)
    {
        return;
    }

    bIsVisible = bNewVisible;
    bVisibleInEditor = bNewVisible;
    bVisibleInGame = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetVisibleInEditor(bool bNewVisible)
{
    if (bVisibleInEditor == bNewVisible)
    {
        return;
    }

    bVisibleInEditor = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetVisibleInGame(bool bNewVisible)
{
    if (bVisibleInGame == bNewVisible)
    {
        return;
    }

    bVisibleInGame = bNewVisible;
    MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetEditorHelper(bool bNewHelper)
{
    if (bIsEditorHelper == bNewHelper)
    {
        return;
    }

    bIsEditorHelper = bNewHelper;
    MarkRenderVisibilityDirty();
}

bool UPrimitiveComponent::ShouldRenderInWorld(EWorldType WorldType) const
{
    if (!bIsVisible)
    {
        return false;
    }

    switch (WorldType)
    {
    case EWorldType::Editor:
        return bVisibleInEditor;
    case EWorldType::PIE:
    case EWorldType::Game:
        return bVisibleInGame;
    default:
        return bVisibleInGame;
    }
}

bool UPrimitiveComponent::ShouldRenderInCurrentWorld() const
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
    return ShouldRenderInWorld(World ? World->GetWorldType() : EWorldType::Game);
}

// ============================================================
// MarkRenderTransformDirty / MarkRenderVisibilityDirty
// ============================================================
void UPrimitiveComponent::MarkRenderTransformDirty()
{
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateActorInOctree(OwnerActor);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::MarkRenderVisibilityDirty()
{
    MarkProxyDirty(ESceneProxyDirtyFlag::Visibility);

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
        return;
    UWorld* World = OwnerActor->GetWorld();
    if (!World)
        return;

    World->UpdateActorInOctree(OwnerActor);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
    OutProps.push_back({ "Visible In Editor", EPropertyType::Bool, &bVisibleInEditor });
    OutProps.push_back({ "Visible In Game", EPropertyType::Bool, &bVisibleInGame });
    OutProps.push_back({ "Is Editor Helper", EPropertyType::Bool, &bIsEditorHelper });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Visible") == 0)
    {
        bVisibleInEditor = bIsVisible;
        bVisibleInGame = bIsVisible;
        MarkRenderVisibilityDirty();
    }
    else if (strcmp(PropertyName, "Visible In Editor") == 0 || strcmp(PropertyName, "Visible In Game") == 0 || strcmp(PropertyName, "Is Editor Helper") == 0)
    {
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

/*
    현재 지원하지 않는 기본 머티리얼 폴백 지점입니다.
*/
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

    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
}

FPrimitiveProxy* UPrimitiveComponent::CreateSceneProxy()
{
    return new FPrimitiveProxy(this);
}

void UPrimitiveComponent::CreateRenderState()
{
    if (!Owner || !Owner->GetWorld())
        return;

    UWorld* World = Owner->GetWorld();

    if (!SceneProxy)
    {
        FScene& Scene = World->GetScene();
        SceneProxy = Scene.AddPrimitive(this);
    }

    World->GetPartition().AddSinglePrimitive(this);
    World->MarkEditorPickingAndScenePrimitiveBVHsDirty();
}

void UPrimitiveComponent::DestroyRenderState()
{
    if (Owner)
    {
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetPartition().RemoveSinglePrimitive(this);
            World->MarkEditorPickingAndScenePrimitiveBVHsDirty();

            if (SceneProxy)
            {
                World->GetScene().RemovePrimitive(SceneProxy);
            }
        }
    }
    SceneProxy = nullptr;
}

void UPrimitiveComponent::BroadcastComponentBeginOverlap(AActor* OtherActor)
{
    OnComponentBeginOverlap.BroadCast(this, OtherActor);
}

void UPrimitiveComponent::BroadcastComponentEndOverlap(AActor* OtherActor)
{
    OnComponentEndOverlap.BroadCast(this, OtherActor);
}

void UPrimitiveComponent::BroadcastComponentHit(AActor* OtherActor)
{
    OnComponentHit.BroadCast(this, OtherActor);
}

bool UPrimitiveComponent::IsOverLappingActor(const AActor* Other)
{
    if (!Other)
        return false;

    // 현재 겹쳐있는 대상들의 목록을 순회하면서, 상대방 액터가 내가 찾는 액터인지 검사
    for (const FOverlapInfo& Info : OverlapInfos)
    {
        if (Info.OverlapActor == Other)
        {
            return true;
        }
    }
    return false;
}

void UPrimitiveComponent::AddOverlapInfo(UPrimitiveComponent* OtherComp)
{
    if (!OtherComp)
        return;

    FOverlapInfo NewInfo;
    // 상대 컴포넌트가 소속된 액터를 가져옵니다. (언리얼의 GetOwner() 같은 함수가 있다고 가정)
    NewInfo.OverlapActor = OtherComp->GetOwner();
    NewInfo.OverlapComponent = OtherComp;

    // 중복 추가 방지 (혹시 모를 예외 처리)
    auto it = std::find(OverlapInfos.begin(), OverlapInfos.end(), NewInfo);
    if (it == OverlapInfos.end())
    {
        OverlapInfos.push_back(NewInfo); // TArray라면 Add()
    }
}

void UPrimitiveComponent::RemoveOverlapInfo(UPrimitiveComponent* OtherComp)
{
    if (!OtherComp)
        return;

    // OtherComp와 일치하는 정보만 배열에서 찾아서 삭제합니다.
    OverlapInfos.erase(
        std::remove_if(OverlapInfos.begin(), OverlapInfos.end(),
                       [OtherComp](const FOverlapInfo& Info)
                       {
                           return Info.OverlapComponent == OtherComp;
                       }),
        OverlapInfos.end());
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
    DestroyRenderState();
    CreateRenderState();
}

void UPrimitiveComponent::OnComponentOverlap(UPrimitiveComponent* Other) const
{
}

void UPrimitiveComponent::OnTransformDirty()
{
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
