#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Containers/Map.h"
#include "Engine/Geometry/AABB.h"
#include "Spatial/BVH.h"

class AActor;
class UPrimitiveComponent;
class UWorld;

/**
 * @brief World-level owner for primitive-to-index mappings, AABB snapshots, and a BVH.
 *
 * Components remain the source of truth for world-space bounds. This class keeps
 * a stable object-index space on top of those components so `FBVH` can operate
 * on a contiguous `TArray<FAABB>`-style interface without depending on scene types.
 */
class FWorldSpatialIndex
{
  public:
    FWorldSpatialIndex() = default;
    ~FWorldSpatialIndex() = default;

    /** @brief Remove all tracked primitives, bounds snapshots, and BVH state. */
    void Clear();

    /**
     * @brief Rebuild the spatial index from every primitive currently owned by the world.
     * @param World World to scan.
     */
    void Rebuild(UWorld* World);

    /** @brief Register every primitive component owned by an actor. */
    void RegisterActor(AActor* Actor);

    /** @brief Unregister every primitive component owned by an actor. */
    void UnregisterActor(AActor* Actor);

    /** @brief Start tracking one primitive component. */
    void RegisterPrimitive(UPrimitiveComponent* Primitive);

    /** @brief Stop tracking one primitive component. */
    void UnregisterPrimitive(UPrimitiveComponent* Primitive);

    /**
     * @brief Mark one primitive for bounds/visibility refresh on the next flush.
     * @param Primitive Primitive to refresh.
     */
    void MarkPrimitiveDirty(UPrimitiveComponent* Primitive);

    /**
     * @brief Apply pending dirty updates to the bounds snapshot and BVH.
     *
     * Visible primitives with valid bounds are kept in the BVH. Invisible or
     * invalid primitives remain tracked but are temporarily removed from the tree.
     */
    void FlushDirtyBounds();

    /** @brief Resolve a tracked object index back to its primitive component. */
    UPrimitiveComponent* Resolve(int32 ObjectIndex) const;

    /** @brief Return the tracked object index for a primitive, or `FBVH::INDEX_NONE`. */
    int32 FindObjectIndex(const UPrimitiveComponent* Primitive) const;

    /** @brief Raw bounds snapshot aligned with BVH object indices. */
    const TArray<FAABB>& GetBounds() const { return Bounds; }

    /** @brief Raw primitive pointer table aligned with BVH object indices. */
    const TArray<UPrimitiveComponent*>& GetPrimitives() const { return Primitives; }

    /** @brief Access the world BVH. */
    FBVH& GetBVH() { return BVH; }

    /** @brief Access the world BVH. */
    const FBVH& GetBVH() const { return BVH; }

  private:
    int32 AllocateObjectIndex();
    void ReleaseObjectIndex(int32 ObjectIndex);

    /** @brief Whether this primitive should be tracked at all. */
    bool ShouldTrackPrimitive(const UPrimitiveComponent* Primitive) const;

    /** @brief Whether this tracked primitive should currently exist inside the BVH. */
    bool ShouldInsertIntoBVH(const UPrimitiveComponent* Primitive, const FAABB& BoundsSnapshot) const;

  private:
    FBVH BVH;

    TArray<UPrimitiveComponent*> Primitives;
    TArray<FAABB> Bounds;
    TArray<uint8> InBVH;
    TArray<uint8> DirtyMarks;

    TArray<int32> DirtyObjectIndices;
    TArray<int32> FreeObjectIndices;

    TMap<UPrimitiveComponent*, int32> PrimitiveToIndex;
};
