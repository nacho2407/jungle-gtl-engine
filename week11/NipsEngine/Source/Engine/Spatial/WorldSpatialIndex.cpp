#include "WorldSpatialIndex.h"

#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

void FWorldSpatialIndex::Clear()
{
    BVH.Reset();
    Primitives.clear();
    Bounds.clear();
    InBVH.clear();
    DirtyMarks.clear();
    DirtyObjectIndices.clear();
    FreeObjectIndices.clear();
    PrimitiveToIndex.clear();
}

void FWorldSpatialIndex::Rebuild(UWorld* World)
{
    Clear();

    if (World == nullptr)
    {
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        RegisterActor(Actor);
    }

    FlushDirtyBounds();
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
        InBVH[ObjectIndex] = 0u;
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
                InBVH[ObjectIndex] = 0u;
            }

            Bounds[ObjectIndex] = NewBounds;
            continue;
        }

        if (!bCurrentlyInBVH)
        {
            Bounds[ObjectIndex] = NewBounds;
            const int32 LeafNodeIndex = BVH.InsertObject(Bounds, ObjectIndex);
            InBVH[ObjectIndex] = (LeafNodeIndex != FBVH::INDEX_NONE) ? 1u : 0u;
            continue;
        }

        if (Bounds[ObjectIndex].NearlyEqualAABB(NewBounds))
        {
            Bounds[ObjectIndex] = NewBounds;
            continue;
        }

        Bounds[ObjectIndex] = NewBounds;
        const bool bUpdated = BVH.UpdateObject(Bounds, ObjectIndex);
        if (!bUpdated)
        {
            const bool bRemoved = BVH.RemoveObject(Bounds, ObjectIndex);
            (void)bRemoved;
            const int32 LeafNodeIndex = BVH.InsertObject(Bounds, ObjectIndex);
            InBVH[ObjectIndex] = (LeafNodeIndex != FBVH::INDEX_NONE) ? 1u : 0u;
        }
    }

    DirtyObjectIndices.clear();
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
