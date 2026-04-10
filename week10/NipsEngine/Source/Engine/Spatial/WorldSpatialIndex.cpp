#include "WorldSpatialIndex.h"

#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Geometry/OBB.h"

void FWorldSpatialIndex::Clear()
{
    BVH.Reset();
    Primitives.clear();
    Bounds.clear();
    InBVH.clear();
    DirtyMarks.clear();
    DirtyObjectIndices.clear();
    FreeObjectIndices.clear();
    BuildObjectIndicesScratch.clear();
    BatchRefitDirtyObjectIndicesScratch.clear();
    PrimitiveToIndex.clear();
    ActiveBVHObjectCount = 0;
}

void FWorldSpatialIndex::Rebuild(UWorld* World)
{
    Clear();

    if (World == nullptr)
    {
        return;
    }

    BuildObjectIndicesScratch.reserve(World->GetActors().size());

    for (AActor* Actor : World->GetActors())
    {
        if (Actor == nullptr)
        {
            continue;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (!ShouldTrackPrimitive(Primitive))
            {
                continue;
            }

            const int32 ObjectIndex = AllocateObjectIndex();
            PrimitiveToIndex[Primitive] = ObjectIndex;
            Primitives[ObjectIndex] = Primitive;
            DirtyMarks[ObjectIndex] = 0u;

            const FAABB BoundsSnapshot = Primitive->GetWorldAABB();
            Bounds[ObjectIndex] = BoundsSnapshot;

            const bool bShouldBeInBVH = ShouldInsertIntoBVH(Primitive, BoundsSnapshot);
            SetInBVHState(ObjectIndex, bShouldBeInBVH);
            if (bShouldBeInBVH)
            {
                BuildObjectIndicesScratch.push_back(ObjectIndex);
            }
        }
    }

    BVH.BuildBVH(Bounds, BuildObjectIndicesScratch);
    DirtyObjectIndices.clear();
}

void FWorldSpatialIndex::RegisterActor(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
    {
        RegisterPrimitive(Primitive);
    }
}

void FWorldSpatialIndex::UnregisterActor(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
    {
        UnregisterPrimitive(Primitive);
    }
}

void FWorldSpatialIndex::RegisterPrimitive(UPrimitiveComponent* Primitive)
{
    if (!ShouldTrackPrimitive(Primitive))
    {
        return;
    }

    if (PrimitiveToIndex.find(Primitive) != PrimitiveToIndex.end())
    {
        MarkPrimitiveDirty(Primitive);
        return;
    }

    const int32 ObjectIndex = AllocateObjectIndex();
    PrimitiveToIndex[Primitive] = ObjectIndex;
    Primitives[ObjectIndex] = Primitive;
    Bounds[ObjectIndex].Reset();
    InBVH[ObjectIndex] = 0u;
    DirtyMarks[ObjectIndex] = 0u;

    MarkPrimitiveDirty(Primitive);
}

void FWorldSpatialIndex::UnregisterPrimitive(UPrimitiveComponent* Primitive)
{
    if (Primitive == nullptr)
    {
        return;
    }

    auto It = PrimitiveToIndex.find(Primitive);
    if (It == PrimitiveToIndex.end())
    {
        return;
    }

    const int32 ObjectIndex = It->second;
    if (ObjectIndex >= 0 && ObjectIndex < static_cast<int32>(InBVH.size()) && InBVH[ObjectIndex] != 0u)
    {
        (void)BVH.RemoveObject(Bounds, ObjectIndex);
        SetInBVHState(ObjectIndex, false);
    }

    PrimitiveToIndex.erase(It);

    if (ObjectIndex >= 0 && ObjectIndex < static_cast<int32>(Primitives.size()))
    {
        Primitives[ObjectIndex] = nullptr;
        Bounds[ObjectIndex].Reset();
        DirtyMarks[ObjectIndex] = 0u;
        ReleaseObjectIndex(ObjectIndex);
    }
}

void FWorldSpatialIndex::MarkPrimitiveDirty(UPrimitiveComponent* Primitive)
{
    if (Primitive == nullptr)
    {
        return;
    }

    const auto It = PrimitiveToIndex.find(Primitive);
    if (It == PrimitiveToIndex.end())
    {
        return;
    }

    const int32 ObjectIndex = It->second;
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(DirtyMarks.size()))
    {
        return;
    }

    if (DirtyMarks[ObjectIndex] == 0u)
    {
        DirtyMarks[ObjectIndex] = 1u;
        DirtyObjectIndices.push_back(ObjectIndex);
    }
}

void FWorldSpatialIndex::FlushDirtyBounds()
{
    if (DirtyObjectIndices.empty())
    {
        return;
    }

    const int32 TotalDirtyCount = static_cast<int32>(DirtyObjectIndices.size());
    int32       StructuralChangeCount = 0;
    BatchRefitDirtyObjectIndicesScratch.clear();
    BatchRefitDirtyObjectIndicesScratch.reserve(DirtyObjectIndices.size());

    for (int32 ObjectIndex : DirtyObjectIndices)
    {
        if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(Primitives.size()))
        {
            continue;
        }

        DirtyMarks[ObjectIndex] = 0u;

        UPrimitiveComponent* Primitive = Primitives[ObjectIndex];
        if (Primitive == nullptr)
        {
            continue;
        }

        const FAABB NewBounds = Primitive->GetWorldAABB();
        const bool  bShouldBeInBVH = ShouldInsertIntoBVH(Primitive, NewBounds);
        const bool  bCurrentlyInBVH = (InBVH[ObjectIndex] != 0u);

        if (!bShouldBeInBVH)
        {
            if (bCurrentlyInBVH)
            {
                (void)BVH.RemoveObject(Bounds, ObjectIndex);
                SetInBVHState(ObjectIndex, false);
                ++StructuralChangeCount;
            }

            Bounds[ObjectIndex] = NewBounds;
            continue;
        }

        if (!bCurrentlyInBVH)
        {
            Bounds[ObjectIndex] = NewBounds;
            const int32 LeafNodeIndex = BVH.InsertObject(Bounds, ObjectIndex);
            const bool bInserted = (LeafNodeIndex != FBVH::INDEX_NONE);
            SetInBVHState(ObjectIndex, bInserted);
            if (bInserted)
            {
                ++StructuralChangeCount;
            }
            continue;
        }

        if (Bounds[ObjectIndex].NearlyEqualAABB(NewBounds))
        {
            Bounds[ObjectIndex] = NewBounds;
            continue;
        }

        Bounds[ObjectIndex] = NewBounds;
        BatchRefitDirtyObjectIndicesScratch.push_back(ObjectIndex);
    }

    const bool bUseBatchRefit = ShouldUseBatchRefit(static_cast<int32>(BatchRefitDirtyObjectIndicesScratch.size()));
    bool       bUsedBatchRefit = false;

    if (bUseBatchRefit)
    {
        BVH.RefitBVH(Bounds, BatchRefitDirtyObjectIndicesScratch);
        bUsedBatchRefit = true;
    }
    else
    {
        for (int32 ObjectIndex : BatchRefitDirtyObjectIndicesScratch)
        {
            const bool bUpdated = BVH.UpdateObject(Bounds, ObjectIndex);
            if (!bUpdated)
            {
                const bool bRemoved = BVH.RemoveObject(Bounds, ObjectIndex);
                (void)bRemoved;
                const int32 LeafNodeIndex = BVH.InsertObject(Bounds, ObjectIndex);
                const bool  bInserted = (LeafNodeIndex != FBVH::INDEX_NONE);
                SetInBVHState(ObjectIndex, bInserted);
                if (bInserted)
                {
                    ++StructuralChangeCount;
                }
            }
        }
    }

    if (ShouldRunRotation(TotalDirtyCount, StructuralChangeCount, bUsedBatchRefit))
    {
        BVH.RotationBVH(Bounds);
    }

    DirtyObjectIndices.clear();
}

bool FWorldSpatialIndex::RayQueryClosestPrimitive(const FRay& Ray, UPrimitiveComponent*& OutPrimitive, float& OutT,
                                                  FBVH::FRayQueryScratch& Scratch)
{
    FlushDirtyBounds();

    OutPrimitive = nullptr;
    OutT = 0.0f;

    int32 ObjectIndex = FBVH::INDEX_NONE;
    if (!BVH.RayQueryClosestAABB(Ray, ObjectIndex, OutT, Scratch))
    {
        return false;
    }

    OutPrimitive = Resolve(ObjectIndex);
    return OutPrimitive != nullptr;
}

void FWorldSpatialIndex::RayQueryPrimitives(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives,
                                            TArray<float>& OutTs, FPrimitiveRayQueryScratch& Scratch)
{
    FlushDirtyBounds();

    Scratch.ObjectIndices.clear();
    Scratch.HitTs.clear();
    BVH.RayQuery(Bounds, Ray, Scratch.ObjectIndices, Scratch.HitTs, Scratch.BVHScratch);

    OutPrimitives.clear();
    OutTs.clear();
    OutPrimitives.reserve(Scratch.ObjectIndices.size());
    OutTs.reserve(Scratch.HitTs.size());

    for (int32 HitIndex = 0; HitIndex < static_cast<int32>(Scratch.ObjectIndices.size()); ++HitIndex)
    {
        if (UPrimitiveComponent* Primitive = Resolve(Scratch.ObjectIndices[HitIndex]))
        {
            OutPrimitives.push_back(Primitive);
            OutTs.push_back(Scratch.HitTs[HitIndex]);
        }
    }
}

void FWorldSpatialIndex::FrustumQueryPrimitives(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives,
                                                FPrimitiveFrustumQueryScratch& Scratch, bool bInsideOnly)
{
    FlushDirtyBounds();

    Scratch.ObjectIndices.clear();
    BVH.FrustumQuery(Frustum, Scratch.ObjectIndices, Scratch.BVHScratch, bInsideOnly);

    OutPrimitives.clear();
    OutPrimitives.reserve(Scratch.ObjectIndices.size());

    for (int32 ObjectIndex : Scratch.ObjectIndices)
    {
        if (UPrimitiveComponent* Primitive = Resolve(ObjectIndex))
        {
            OutPrimitives.push_back(Primitive);
        }
    }
}

UPrimitiveComponent* FWorldSpatialIndex::Resolve(int32 ObjectIndex) const
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(Primitives.size()))
    {
        return nullptr;
    }

    return Primitives[ObjectIndex];
}

int32 FWorldSpatialIndex::FindObjectIndex(const UPrimitiveComponent* Primitive) const
{
    if (Primitive == nullptr)
    {
        return FBVH::INDEX_NONE;
    }

    const auto It = PrimitiveToIndex.find(const_cast<UPrimitiveComponent*>(Primitive));
    return (It != PrimitiveToIndex.end()) ? It->second : FBVH::INDEX_NONE;
}

int32 FWorldSpatialIndex::AllocateObjectIndex()
{
    if (!FreeObjectIndices.empty())
    {
        const int32 ObjectIndex = FreeObjectIndices.back();
        FreeObjectIndices.pop_back();
        return ObjectIndex;
    }

    const int32 ObjectIndex = static_cast<int32>(Primitives.size());
    Primitives.push_back(nullptr);
    Bounds.emplace_back();
    Bounds.back().Reset();
    InBVH.push_back(0u);
    DirtyMarks.push_back(0u);
    return ObjectIndex;
}

void FWorldSpatialIndex::ReleaseObjectIndex(int32 ObjectIndex)
{
    if (ObjectIndex < 0)
    {
        return;
    }

    FreeObjectIndices.push_back(ObjectIndex);
}

void FWorldSpatialIndex::SetInBVHState(int32 ObjectIndex, bool bInBVH)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(InBVH.size()))
    {
        return;
    }

    const uint8 NewValue = bInBVH ? 1u : 0u;
    if (InBVH[ObjectIndex] == NewValue)
    {
        return;
    }

    InBVH[ObjectIndex] = NewValue;
    ActiveBVHObjectCount += bInBVH ? 1 : -1;
    if (ActiveBVHObjectCount < 0)
    {
        ActiveBVHObjectCount = 0;
    }
}

bool FWorldSpatialIndex::ShouldUseBatchRefit(int32 DirtyUpdateCount) const
{
    if (DirtyUpdateCount <= 0 || ActiveBVHObjectCount <= 0)
    {
        return false;
    }

    return DirtyUpdateCount >= MaintenancePolicy.BatchRefitMinDirtyCount &&
           (DirtyUpdateCount * 100) >= (ActiveBVHObjectCount * MaintenancePolicy.BatchRefitDirtyPercentThreshold);
}

bool FWorldSpatialIndex::ShouldRunRotation(int32 TotalDirtyCount, int32 StructuralChangeCount,
                                           bool bUsedBatchRefit) const
{
    if (ActiveBVHObjectCount < 2)
    {
        return false;
    }

    if (StructuralChangeCount >= MaintenancePolicy.RotationStructuralChangeThreshold)
    {
        return true;
    }

    if (bUsedBatchRefit)
    {
        return TotalDirtyCount >= MaintenancePolicy.RotationDirtyCountThreshold ||
               (TotalDirtyCount * 100) >= (ActiveBVHObjectCount * MaintenancePolicy.RotationDirtyPercentThreshold);
    }

    return false;
}

bool FWorldSpatialIndex::ShouldTrackPrimitive(const UPrimitiveComponent* Primitive) const
{
    return Primitive != nullptr && Primitive->GetOwner() != nullptr;
}

bool FWorldSpatialIndex::ShouldInsertIntoBVH(const UPrimitiveComponent* Primitive, const FAABB& BoundsSnapshot) const
{
    if (!ShouldTrackPrimitive(Primitive) || !BoundsSnapshot.IsValid())
    {
        return false;
    }

    const AActor* Owner = Primitive->GetOwner();
    return Owner != nullptr && Owner->IsVisible() && Primitive->IsVisible();
}
