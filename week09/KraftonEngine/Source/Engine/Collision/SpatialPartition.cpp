#include "SpatialPartition.h"

#include "Collision/Octree.h"
#include "Collision/RayUtils.h"
#include "Core/RayTypes.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "GameFramework/AActor.h"
#include <algorithm>

namespace
{
    void ExpandBoundsByBox(FBoundingBox& AccumulatedBounds, const FBoundingBox& Box)
    {
        if (!Box.IsValid())
        {
            return;
        }

        if (!AccumulatedBounds.IsValid())
        {
            AccumulatedBounds = Box;
            return;
        }

        AccumulatedBounds.Expand(Box.Min);
        AccumulatedBounds.Expand(Box.Max);
    }

    FBoundingBox MakePaddedRootBounds(const FBoundingBox& Bounds)
    {
        if (!Bounds.IsValid())
        {
            return Bounds;
        }

        const FVector Extent = Bounds.GetExtent();
        const FVector Padding(
            (std::max)(Extent.X * 0.25f, 10.0f),
            (std::max)(Extent.Y * 0.25f, 10.0f),
            (std::max)(Extent.Z * 0.25f, 10.0f));

        return FBoundingBox(Bounds.Min - Padding, Bounds.Max + Padding);
    }
}

FBoundingBox FSpatialPartition::BuildActorVisibleBounds(AActor* Actor, bool bUpdateWorldMatrices) const
{
    FBoundingBox ActorBounds;

    if (!Actor)
    {
        return ActorBounds;
    }

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim || !Prim->IsVisible())
        {
            continue;
        }

        if (bUpdateWorldMatrices)
        {
            Prim->UpdateWorldMatrix();
        }

        ExpandBoundsByBox(ActorBounds, Prim->GetWorldBoundingBox());
    }

    return ActorBounds;
}

void FSpatialPartition::EnsureRootContains(const FBoundingBox& RequiredBounds)
{
    if (!RequiredBounds.IsValid())
    {
        return;
    }

    if (!Octree)
    {
        Octree = std::make_unique<FOctree>(MakePaddedRootBounds(RequiredBounds), 0);
        return;
    }

    if (Octree->GetCellBounds().IsContains(RequiredBounds))
    {
        return;
    }

    RebuildRootBounds(RequiredBounds);
}

void FSpatialPartition::RebuildRootBounds(const FBoundingBox& RequiredBounds)
{
    if (!Octree || !RequiredBounds.IsValid())
    {
        return;
    }

    TArray<UPrimitiveComponent*> AllPrimitives;
    Octree->GetAllPrimitives(AllPrimitives);
    AllPrimitives.insert(AllPrimitives.end(), OverflowPrimitives.begin(), OverflowPrimitives.end());

    FBoundingBox NewRootBounds = RequiredBounds;
    for (UPrimitiveComponent* Prim : AllPrimitives)
    {
        if (!Prim || !Prim->IsVisible())
        {
            continue;
        }

        ExpandBoundsByBox(NewRootBounds, Prim->GetWorldBoundingBox());
    }

    if (!NewRootBounds.IsValid())
    {
        return;
    }

    NewRootBounds = MakePaddedRootBounds(NewRootBounds);

    Octree->Reset(NewRootBounds, 0);
    OverflowPrimitives.clear();

    for (UPrimitiveComponent* Prim : AllPrimitives)
    {
        if (!Prim || !Prim->IsVisible())
        {
            continue;
        }

        if (!Octree->Insert(Prim))
        {
            InsertPrimitive(Prim);
        }
    }
}

void FSpatialPartition::FlushPrimitive()
{
	if (!Octree) return;

    FBoundingBox DirtyBounds;
    for (AActor* Actor : DirtyActors)
    {
        ExpandBoundsByBox(DirtyBounds, BuildActorVisibleBounds(Actor, true));
    }

    const bool bNeedsRootGrowth = DirtyBounds.IsValid() && !Octree->GetCellBounds().IsContains(DirtyBounds);
    if (bNeedsRootGrowth)
    {
        RebuildRootBounds(DirtyBounds);
        DirtyActors.clear();
        return;
    }

    for (AActor* Actor : DirtyActors)
    {
        if (!Actor) continue;

        // 1. 기존 위치에서 제거
        for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
        {
            if (!Prim) continue;

            Octree->Remove(Prim);
            RemovePrimitive(Prim); // overflow에서도 제거
        }

        // 2. 최신 transform 기준으로 다시 삽입
        for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
        {
            if (!Prim || !Prim->IsVisible()) continue;

            if (!Octree->Insert(Prim))
            {
                InsertPrimitive(Prim);
            }
        }
    }

    DirtyActors.clear();
}

void FSpatialPartition::Reset(const FBoundingBox& RootBounds)
{
    if (!RootBounds.IsValid())
    {
        Octree.reset();
    }
    else
    {
        if (!Octree)
        {
            Octree = std::make_unique<FOctree>(RootBounds, 0);
        }
        else
        {
            Octree->Reset(RootBounds, 0);
        }
    }

	DirtyActors.clear();
    OverflowPrimitives.clear();
}

void FSpatialPartition::InsertActor(AActor* Actor)
{
	if (!Actor) return;

    const FBoundingBox ActorBounds = BuildActorVisibleBounds(Actor, true);
    EnsureRootContains(ActorBounds);

    if (!Octree)
    {
        return;
    }

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim || !Prim->IsVisible()) continue;

        if (!Octree->Insert(Prim)) InsertPrimitive(Prim);
    }
}

void FSpatialPartition::RemoveActor(AActor* Actor)
{
	if (!Actor|| !Octree) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim) continue;

		Octree->Remove(Prim);
        RemovePrimitive(Prim);
    }
}

void FSpatialPartition::UpdateActor(AActor* Actor)
{
	if (!Actor) return;

    if (!Octree)
    {
        InsertActor(Actor);
        return;
    }

    auto It = std::find(DirtyActors.begin(), DirtyActors.end(), Actor);
    if (It == DirtyActors.end())
    {
        DirtyActors.push_back(Actor);
    }
}

void FSpatialPartition::QueryFrustumAllPrimitive(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (Octree)
	{
		Octree->QueryFrustum(ConvexVolume, OutPrimitives);
	}

	for (UPrimitiveComponent* Prim : OverflowPrimitives)
	{
		if (!Prim || !Prim->IsVisible()) continue;

		if (ConvexVolume.IntersectAABB(Prim->GetWorldBoundingBox()))
		{
			OutPrimitives.push_back(Prim);
		}
	}
}

void FSpatialPartition::QueryFrustumAllProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies) const
{
	if (Octree)
	{
		Octree->QueryFrustumProxies(ConvexVolume, OutProxies);
	}

	for (UPrimitiveComponent* Prim : OverflowPrimitives)
	{
		if (!Prim || !Prim->IsVisible()) continue;

		if (ConvexVolume.IntersectAABB(Prim->GetWorldBoundingBox()))
		{
			if (FPrimitiveSceneProxy* Proxy = Prim->GetSceneProxy())
				OutProxies.push_back(Proxy);
		}
	}
}

void FSpatialPartition::QueryRayAllPrimitive(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (Octree)
	{
		Octree->QueryRay(Ray, OutPrimitives);
	}

	for (UPrimitiveComponent* Prim : OverflowPrimitives)
	{
		if (!Prim || !Prim->IsVisible()) continue;

		const FBoundingBox Box = Prim->GetWorldBoundingBox();
		if (FRayUtils::CheckRayAABB(Ray, Box.Min, Box.Max))
		{
			OutPrimitives.push_back(Prim);
		}
	}
}

void FSpatialPartition::InsertPrimitive(UPrimitiveComponent* Primitive)
{
	if (!Primitive) return;

	auto It = std::find(OverflowPrimitives.begin(), OverflowPrimitives.end(), Primitive);
	if (It == OverflowPrimitives.end())
	{
		OverflowPrimitives.push_back(Primitive);
	}
}

void FSpatialPartition::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	if (!Primitive) return;

	auto It = std::find(OverflowPrimitives.begin(), OverflowPrimitives.end(), Primitive);
	if (It != OverflowPrimitives.end())
	{
		*It = OverflowPrimitives.back();
		OverflowPrimitives.pop_back();
	}
}
