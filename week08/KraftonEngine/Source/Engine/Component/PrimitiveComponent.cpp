#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
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
    // LocalExtents??硫붿떆 ?깆뿉???ш퀎?곕릺誘濡?吏곷젹???쒖쇅.
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
//   ?꾨줉??dirty + Octree(?≫꽣 ?⑥쐞 dirty) + PickingBVH dirty
//   ?몄텧?먭? ?몄썙???덈뜕 ?쒗?ㅻ? ?⑥씪 吏꾩엯?먯쑝濡??듯빀.
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
    World->MarkWorldPrimitivePickingBVHDirty();
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

    // 媛?쒖꽦 蹂?붾뒗 Octree ?ы븿 ?щ???醫뚯슦?섎?濡??≫꽣 dirty濡?諛섏쁺?쒕떎.
    World->UpdateActorInOctree(OwnerActor);
    World->MarkWorldPrimitivePickingBVHDirty();
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
    // 踰좎씠???대옒?ㅼ쓽 transform ??怨듯넻 ?꾨줈?쇳떚 泥섎━ 蹂댁옣
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
    // Local bounds(shape) ?먯껜媛 諛붾?寃쎌슦??吏꾩엯??
    // fast-path(?댁쟾 AABB瑜?translation留뚯쑝濡??ъ궗????shape媛 ?숈씪?섎떎??媛?뺤뿉 ?섏〈?섎?濡?
    // ?ш린?쒕뒗 諛섎뱶??臾대젰?뷀빐???쒕떎. ??洹몃윭硫?mesh 援먯껜 ?꾩뿉??stale AABB媛 罹먯떆?쒕떎.
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

    // ?꾨줉?쒓? ?깅줉??寃쎌슦 Transform dirty ?꾪뙆 (FScene DirtySet?먮룄 ?깅줉)
    MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
}

// --- ?꾨줉???⑺넗由?---
FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
    // 湲곕낯 PrimitiveComponent???꾨줉??
    return new FPrimitiveSceneProxy(this);
}

// --- ?뚮뜑 ?곹깭 愿由?(UE RegisterComponent ??? ---
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

    // Proxy媛 ?대? ?댁븘 ?덉뼱??partition?먯꽌留?鍮좎쭊 ?곹깭媛 ?덉쓣 ???덈떎.
    // render visibility/frustum query??partition 湲곕컲?대?濡??깅줉??idempotent?섍쾶 蹂댁젙?쒕떎.
    World->GetPartition().AddSinglePrimitive(this);
    World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::DestroyRenderState()
{
    // SceneProxy媛 ?녿뜑?쇰룄 Octree?먮뒗 ?깅줉???덉쓣 ???덉쑝誘濡?partition ?뺣━????긽 ?쒕룄?쒕떎.
    if (Owner)
    {
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetPartition().RemoveSinglePrimitive(this);
            World->MarkWorldPrimitivePickingBVHDirty();

            if (SceneProxy)
            {
                // Scene.RemovePrimitive 媛 VisibleProxies 罹먯떆???쇨??섍쾶 ?뺣━?쒕떎.
                World->GetScene().RemovePrimitive(SceneProxy);
            }
        }
    }
    SceneProxy = nullptr;
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
    // ?꾨줉???뚭눼 ???ъ깮????硫붿떆 援먯껜 ????蹂寃????ъ슜
    DestroyRenderState();
    CreateRenderState();
}

void UPrimitiveComponent::OnTransformDirty()
{
    // ?쒖닔 transform 蹂寃???local bounds(shape)??洹몃?濡쒖씠誘濡?fast-path瑜??대┛??
    // (basis ?숈씪 + translation留?諛붾?寃쎌슦 UpdateWorldMatrix媛 ?댁쟾 AABB瑜??됲뻾?대룞留??곸슜)
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
