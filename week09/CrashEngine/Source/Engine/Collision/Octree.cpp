// 충돌/피킹 영역의 세부 동작을 구현합니다.
#include "Octree.h"
#include <Collision/RayUtils.h>
#include <algorithm>
#include "Sphere.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

namespace
{
int GetChildIndex(const FVector& Center, const FVector& NodeCenter)
{
    // Bit layout:
    // bit0 = +X, bit1 = +Y, bit2 = +Z
    int idx = 0;
    if (Center.X >= NodeCenter.X)
        idx |= 1;
    if (Center.Y >= NodeCenter.Y)
        idx |= 2;
    if (Center.Z >= NodeCenter.Z)
        idx |= 4;
    return idx;
}

FBoundingBox MakeLooseBounds(const FBoundingBox& CellBounds)
{
    const FVector C = CellBounds.GetCenter();
    const FVector E = CellBounds.GetExtent() * LooseFactor;
    return FBoundingBox(C - E, C + E);
}
} // namespace

FOctree::FOctree()
    : CellBounds(), LooseBounds(), Depth(0), Parent(nullptr)
{
}

FOctree::FOctree(const FBoundingBox& BoundOctree, const uint32& depth, FOctree* InParent)
    : CellBounds(BoundOctree), LooseBounds(MakeLooseBounds(BoundOctree)), Depth(depth), Parent(InParent)
{
}

FOctree::~FOctree()
{
    ClearChildrenAndPrimitiveLocations();
}

bool FOctree::Insert(UPrimitiveComponent* Primitive)
{
    if (!Primitive)
        return false;

    const FBoundingBox PrimBox = Primitive->GetWorldBoundingBox();
    const FVector PrimCenter = PrimBox.GetCenter();

    // primitive 중심이 안에 들어왔는지 확인한다
    if (!CellBounds.IsContains(PrimCenter))
        return false;

    // ── 내부 노드:
    if (!IsLeaf())
    {
        const int childIndex = GetChildIndex(PrimCenter, CellBounds.GetCenter());
        FOctree* Child = Children[childIndex];

        if (Child && Child->LooseBounds.IsContains(PrimBox))
            return Child->Insert(Primitive);

        PrimitiveList.push_back(Primitive);
        Primitive->SetOctreeLocation(this, false);
        return true;
    }

    // ── 리프 노드 ──
    // 용량 초과 && 깊이 여유 있음 && 실제로 분배 가능한 객체가 존재할 때만 분할
    PrimitiveList.push_back(Primitive);
    Primitive->SetOctreeLocation(this, false);

    if (HasDistributable() && (int)PrimitiveList.size() > MAX_SIZE && Depth < MAX_DEPTH)
    {
        SubDivide();
    }

    return true;
}

bool FOctree::RemoveDirect(UPrimitiveComponent* Primitive, bool bTryMergeNow)
{
    if (!Primitive)
    {
        return false;
    }

    auto It = std::find(PrimitiveList.begin(), PrimitiveList.end(), Primitive);
    if (It == PrimitiveList.end())
    {
        return false;
    }

    if (Primitive->GetOctreeNode() == this)
    {
        Primitive->ClearOctreeLocation();
    }

    *It = PrimitiveList.back();
    PrimitiveList.pop_back();

    if (bTryMergeNow)
    {
        TryMergeUpward();
    }

    return true;
}

void FOctree::TryMergeUpward()
{
    for (FOctree* Node = this; Node; Node = Node->Parent)
    {
        Node->TryMerge();
    }
}

void FOctree::TryMergeRecursive()
{
    if (!IsLeaf())
    {
        // Copy children pointers because TryMerge() may delete and clear Children.
        TArray<FOctree*> Snapshot = Children;
        for (FOctree* Child : Snapshot)
        {
            if (Child)
            {
                Child->TryMergeRecursive();
            }
        }
    }
    TryMerge();
}

bool FOctree::Remove(UPrimitiveComponent* Primitive)
{
    if (!Primitive)
    {
        return false;
    }

    if (Primitive->GetOctreeNode() == this)
    {
        return RemoveDirect(Primitive, true);
    }

    if (!IsLeaf())
    {
        for (FOctree* Child : Children)
        {
            if (Child && Child->Remove(Primitive))
            {
                return true;
            }
        }
    }

    return RemoveDirect(Primitive, true);
}

void FOctree::TryMerge()
{
    if (IsLeaf())
    {
        return;
    }

    int32 TotalPrimitives = static_cast<int32>(PrimitiveList.size());
    for (FOctree* Child : Children)
    {
        if (!Child)
        {
            continue;
        }

        if (!Child->Children.empty())
        {
            return;
        }

        TotalPrimitives += static_cast<int32>(Child->PrimitiveList.size());
    }

    if (TotalPrimitives > MAX_SIZE)
    {
        return;
    }

    for (FOctree* Child : Children)
    {
        if (!Child)
        {
            continue;
        }

        for (UPrimitiveComponent* Prim : Child->PrimitiveList)
        {
            if (Prim)
            {
                Prim->SetOctreeLocation(this, false);
            }
        }

        PrimitiveList.insert(
            PrimitiveList.end(),
            Child->PrimitiveList.begin(),
            Child->PrimitiveList.end());

        Child->PrimitiveList.clear();
        delete Child;
    }

    Children.clear();
}
void FOctree::ClearChildrenAndPrimitiveLocations()
{
    for (UPrimitiveComponent* Prim : PrimitiveList)
    {
        if (Prim && Prim->GetOctreeNode() == this)
        {
            Prim->ClearOctreeLocation();
        }
    }
    PrimitiveList.clear();

    for (FOctree* Child : Children)
    {
        if (Child)
        {
            Child->ClearChildrenAndPrimitiveLocations();
            delete Child;
        }
    }
    Children.clear();
}

void FOctree::SubDivide()
{
    if (!IsLeaf())
        return;

    const FVector Center = CellBounds.GetCenter();
    const FVector Min = CellBounds.Min;
    const FVector Max = CellBounds.Max;

    const FBoundingBox ChildBoxes[8] = {
        { FVector(Min.X, Min.Y, Min.Z), FVector(Center.X, Center.Y, Center.Z) },
        { FVector(Center.X, Min.Y, Min.Z), FVector(Max.X, Center.Y, Center.Z) },
        { FVector(Min.X, Center.Y, Min.Z), FVector(Center.X, Max.Y, Center.Z) },
        { FVector(Center.X, Center.Y, Min.Z), FVector(Max.X, Max.Y, Center.Z) },
        { FVector(Min.X, Min.Y, Center.Z), FVector(Center.X, Center.Y, Max.Z) },
        { FVector(Center.X, Min.Y, Center.Z), FVector(Max.X, Center.Y, Max.Z) },
        { FVector(Min.X, Center.Y, Center.Z), FVector(Center.X, Max.Y, Max.Z) },
        { FVector(Center.X, Center.Y, Center.Z), FVector(Max.X, Max.Y, Max.Z) },
    };

    Children.resize(8, nullptr);
    for (int i = 0; i < 8; ++i)
        Children[i] = new FOctree(ChildBoxes[i], Depth + 1, this);

    TArray<UPrimitiveComponent*> primitivesToMove = PrimitiveList;
    PrimitiveList.clear();

    int Distributed = 0;
    for (UPrimitiveComponent* Prim : primitivesToMove)
    {
        // Insert 재귀 대신 직접 자식에 배분
        // → 이미 내부 노드이므로 Insert의 "내부 노드" 분기를 타게 됨
        // → 어느 자식에도 안 들어가면 PrimitiveList에 크로스-바운더리로 남음
        // → 크로스-바운더리는 더 이상 SubDivide를 유발하지 않음 (CountDistributable 조건)
        bool bPlaced = false;
        const FBoundingBox PrimBox = Prim->GetWorldBoundingBox();
        const FVector PrimCenter = PrimBox.GetCenter();

        const int ChildIndex = GetChildIndex(PrimCenter, Center);
        FOctree* Child = Children[ChildIndex];

        if (Child && Child->LooseBounds.IsContains(PrimBox))
        {
            Child->Insert(Prim);
            bPlaced = true;
            ++Distributed;
        }

        if (!bPlaced)
        {
            PrimitiveList.push_back(Prim);
            Prim->SetOctreeLocation(this, false);
        }
    }

    if (Distributed == 0)
    {
        for (FOctree* Child : Children)
            delete Child;
        Children.clear();
        // PrimitiveList는 이미 위에서 크로스-바운더리로 재구성됨
    }
}

bool FOctree::HasPrimitive(const UPrimitiveComponent* Primitive)
{
    for (UPrimitiveComponent* Prim : PrimitiveList)
    {
        if (Prim == Primitive)
        {
            return true;
        }
    }

    if (IsLeaf())
    {
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i]->HasPrimitive(Primitive))
        {
            return true;
        }
    }

    return false;
}

void FOctree::GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitiveList)
{
    OutPrimitiveList.insert(OutPrimitiveList.end(), PrimitiveList.begin(), PrimitiveList.end());

    if (!IsLeaf())
    {
        for (int Index = 0; Index < 8; ++Index)
        {
            Children[Index]->GetAllPrimitives(OutPrimitiveList);
        }
    }
}

TArray<UPrimitiveComponent*> FOctree::FindNearestPrimitiveList(const FVector& Pos, const FVector& QueryExtent, uint32 Count)
{
    TArray<UPrimitiveComponent*> Candidates;
    const FBoundingBox QueryBox(Pos - QueryExtent, Pos + QueryExtent);

    QueryAABB(QueryBox, Candidates);

    std::sort(Candidates.begin(), Candidates.end(),
              [&Pos](UPrimitiveComponent* A, UPrimitiveComponent* B)
              {
                  return A->GetWorldBoundingBox().GetCenterDistanceSquared(Pos) < B->GetWorldBoundingBox().GetCenterDistanceSquared(Pos);
              });

    if (Candidates.size() > Count)
    {
        Candidates.resize(Count);
    }

    return Candidates;
}

void FOctree::QueryAABB(const FBoundingBox& QueryBox, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
    if (!LooseBounds.IsIntersected(QueryBox))
        return;

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (Primitive && Primitive->GetWorldBoundingBox().IsIntersected(QueryBox))
        {
            OutPrimitives.push_back(Primitive);
        }
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
    {
        Children[i]->QueryAABB(QueryBox, OutPrimitives);
    }
}

void FOctree::QueryRay(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
    if (!FRayUtils::CheckRayAABB(Ray, LooseBounds.Min, LooseBounds.Max))
        return;

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        const FBoundingBox& box = Primitive->GetWorldBoundingBox();
        if (Primitive && FRayUtils::CheckRayAABB(Ray, box.Min, box.Max))
        {
            OutPrimitives.push_back(Primitive);
        }
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
    {
        Children[i]->QueryRay(Ray, OutPrimitives);
    }
}

void FOctree::Reset(const FBoundingBox& InBounds, uint32 InDepth)
{
    ClearChildrenAndPrimitiveLocations();

    CellBounds = InBounds;
    LooseBounds = MakeLooseBounds(InBounds);
    Depth = InDepth;
    Parent = nullptr;
}

bool FOctree::HasDistributable() const
{
    const FVector Center = CellBounds.GetCenter();
    const FVector Min = CellBounds.Min;
    const FVector Max = CellBounds.Max;

    const FBoundingBox ChildBoxes[8] = {
        { FVector(Min.X, Min.Y, Min.Z), FVector(Center.X, Center.Y, Center.Z) },
        { FVector(Center.X, Min.Y, Min.Z), FVector(Max.X, Center.Y, Center.Z) },
        { FVector(Min.X, Center.Y, Min.Z), FVector(Center.X, Max.Y, Center.Z) },
        { FVector(Center.X, Center.Y, Min.Z), FVector(Max.X, Max.Y, Center.Z) },
        { FVector(Min.X, Min.Y, Center.Z), FVector(Center.X, Center.Y, Max.Z) },
        { FVector(Center.X, Min.Y, Center.Z), FVector(Max.X, Center.Y, Max.Z) },
        { FVector(Min.X, Center.Y, Center.Z), FVector(Center.X, Max.Y, Max.Z) },
        { FVector(Center.X, Center.Y, Center.Z), FVector(Max.X, Max.Y, Max.Z) },
    };

    for (UPrimitiveComponent* Prim : PrimitiveList)
    {
        if (!Prim)
            continue;

        const FBoundingBox PrimBox = Prim->GetWorldBoundingBox();
        const FVector PrimCenter = PrimBox.GetCenter();
        const int ChildIndex = GetChildIndex(PrimCenter, Center);
        const FBoundingBox ChildLoose = MakeLooseBounds(ChildBoxes[ChildIndex]);

        if (ChildLoose.IsContains(PrimBox))
            return true;
    }
    return false;
}

void FOctree::QueryFrustum(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
    QueryFrustumInternal(ConvexVolume, OutPrimitives, false);
}

void FOctree::CollectAll(TArray<UPrimitiveComponent*>& OutPrimitives) const
{
    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (Primitive)
            OutPrimitives.push_back(Primitive);
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
        Children[i]->CollectAll(OutPrimitives);
}

void FOctree::QueryFrustumInternal(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives, bool bParentContained) const
{
    if (bParentContained)
    {
        CollectAll(OutPrimitives);
        return;
    }

    switch (ConvexVolume.ClassifyAABB(LooseBounds))
    {
    case EAABBFrustumClassify::Outside:
        return;

    case EAABBFrustumClassify::Contains:
        CollectAll(OutPrimitives);
        return;

    case EAABBFrustumClassify::Intersects:
        break;
    }

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (Primitive && ConvexVolume.IntersectAABB(Primitive->GetWorldBoundingBox()))
            OutPrimitives.push_back(Primitive);
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
    {
        Children[i]->QueryFrustumInternal(ConvexVolume, OutPrimitives, false);
    }
}

// ================================================================
// Proxy-direct frustum query ? avoids Component→GetSceneProxy() indirection
// ================================================================

void FOctree::QueryFrustumProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveProxy*>& OutProxies) const
{
    QueryFrustumProxiesInternal(ConvexVolume, OutProxies, false);
}

void FOctree::QuerySphereProxies(FSphere Sphere, TArray<FPrimitiveProxy*>& OutProxies) const
{
    if (!Sphere.IntersectAABB(LooseBounds))
        return;

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (Primitive && Sphere.IntersectAABB(Primitive->GetWorldBoundingBox()))
        {
            if (FPrimitiveProxy* Proxy = Primitive->GetSceneProxy())
            {
                if (!Proxy->bNeverCull)
                    OutProxies.push_back(Proxy);
            }
        }
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
    {
        Children[i]->QuerySphereProxies(Sphere, OutProxies);
    }
}

void FOctree::CollectAllProxies(TArray<FPrimitiveProxy*>& OutProxies) const
{
    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (Primitive)
        {
            if (FPrimitiveProxy* Proxy = Primitive->GetSceneProxy())
                if (!Proxy->bNeverCull)
                    OutProxies.push_back(Proxy);
        }
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
        Children[i]->CollectAllProxies(OutProxies);
}

void FOctree::QueryFrustumProxiesInternal(const FConvexVolume& ConvexVolume, TArray<FPrimitiveProxy*>& OutProxies, bool bParentContained) const
{
    if (bParentContained)
    {
        CollectAllProxies(OutProxies);
        return;
    }

    switch (ConvexVolume.ClassifyAABB(LooseBounds))
    {
    case EAABBFrustumClassify::Outside:
        return;

    case EAABBFrustumClassify::Contains:
        CollectAllProxies(OutProxies);
        return;

    case EAABBFrustumClassify::Intersects:
        break;
    }

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
        if (!Primitive)
            continue;

        if (ConvexVolume.IntersectAABB(Primitive->GetWorldBoundingBox()))
        {
            if (FPrimitiveProxy* Proxy = Primitive->GetSceneProxy())
                if (!Proxy->bNeverCull)
                    OutProxies.push_back(Proxy);
        }
    }

    if (IsLeaf())
        return;

    for (int i = 0; i < 8; ++i)
        Children[i]->QueryFrustumProxiesInternal(ConvexVolume, OutProxies, false);
}

