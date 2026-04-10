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
    /** @brief Tunable policy values that decide when batched maintenance paths should run. */
    struct FMaintenancePolicy
    {
        int32 BatchRefitMinDirtyCount{8};
        int32 BatchRefitDirtyPercentThreshold{15};
        int32 RotationStructuralChangeThreshold{8};
        int32 RotationDirtyCountThreshold{24};
        int32 RotationDirtyPercentThreshold{30};
    };

    /**
     * @brief Caller-owned scratch for primitive frustum queries.
     * @note The wrapper keeps the BVH's integer object-index output hidden from
     * the caller while still avoiding per-query allocations.
     */
    struct FPrimitiveFrustumQueryScratch
    {
        FBVH::FFrustumQueryScratch BVHScratch;
        TArray<int32>             ObjectIndices;
    };

    /**
     * @brief Caller-owned scratch for primitive ray queries that return all BVH leaf hits.
     * @note The wrapper keeps BVH object indices internal while still allowing
     * callers to reuse traversal/sort buffers across repeated queries.
     */
    struct FPrimitiveRayQueryScratch
    {
        FBVH::FRayQueryScratch BVHScratch;
        TArray<int32>         ObjectIndices;
        TArray<float>         HitTs;
    };

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

    /**
     * @brief Return the closest primitive whose BVH leaf AABB is hit by the ray.
     * @param Ray Query ray.
     * @param OutPrimitive Closest intersected primitive, or `nullptr` on miss.
     * @param OutT Ray distance to the closest leaf AABB hit.
     * @param Scratch Caller-owned BVH ray-query scratch.
     * @return `true` if any tracked primitive leaf AABB is hit.
     * @note Pending dirty bounds are flushed before the query executes.
     */
    bool RayQueryClosestPrimitive(const FRay& Ray, UPrimitiveComponent*& OutPrimitive, float& OutT,
                                  FBVH::FRayQueryScratch& Scratch);

    /**
     * @brief Collect primitives whose BVH leaf AABBs are intersected by the ray.
     * @param Ray Query ray.
     * @param OutPrimitives Output primitives sorted by leaf-AABB hit distance.
     * @param OutTs Output hit distances aligned with `OutPrimitives`.
     * @param Scratch Caller-owned wrapper scratch.
     * @note Pending dirty bounds are flushed before the query executes.
     * @note This is still a broad-phase query over leaf AABBs. Callers that need
     * exact mesh picking should perform a narrow-phase primitive raycast on the
     * returned candidates.
     */
    void RayQueryPrimitives(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives, TArray<float>& OutTs,
                            FPrimitiveRayQueryScratch& Scratch);

    /**
     * @brief Collect tracked primitives whose leaf AABBs overlap the input frustum.
     * @param Frustum Query frustum.
     * @param OutPrimitives Output primitive list.
     * @param Scratch Caller-owned wrapper scratch.
     * @param bInsideOnly If `true`, include only primitives fully inside the frustum.
     * @note Pending dirty bounds are flushed before the query executes.
     */
    void FrustumQueryPrimitives(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives,
                                FPrimitiveFrustumQueryScratch& Scratch, bool bInsideOnly = false);


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

    /** @brief Read the current maintenance thresholds used by `FlushDirtyBounds()`. */
    const FMaintenancePolicy& GetMaintenancePolicy() const { return MaintenancePolicy; }

    /** @brief Access the maintenance thresholds for tuning. */
    FMaintenancePolicy& GetMaintenancePolicy() { return MaintenancePolicy; }

  private:
    int32 AllocateObjectIndex();
    void ReleaseObjectIndex(int32 ObjectIndex);
    void SetInBVHState(int32 ObjectIndex, bool bInBVH);
    bool ShouldUseBatchRefit(int32 DirtyUpdateCount) const;
    bool ShouldRunRotation(int32 TotalDirtyCount, int32 StructuralChangeCount, bool bUsedBatchRefit) const;

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
    TArray<int32> BuildObjectIndicesScratch;
    TArray<int32> BatchRefitDirtyObjectIndicesScratch;

    TMap<UPrimitiveComponent*, int32> PrimitiveToIndex;
    int32                             ActiveBVHObjectCount{0};
    FMaintenancePolicy                MaintenancePolicy;
};
