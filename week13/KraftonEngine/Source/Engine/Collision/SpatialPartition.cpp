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

void FSpatialPartition::ClearQueuedActorFlags()
{
	for (AActor* Actor : DirtyActors)
	{
		if (Actor)
		{
			Actor->SetQueuedForPartitionUpdate(false);
		}
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
		if (!Prim)
		{
			continue;
		}

		Prim->ClearOctreeLocation();

		if (Prim->IsVisible())
		{
			ExpandBoundsByBox(NewRootBounds, Prim->GetWorldBoundingBox());
		}
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
	if (DirtyActors.empty())
	{
		return;
	}

	if (!Octree)
	{
		ClearQueuedActorFlags();
		DirtyActors.clear();
		return;
	}

	FBoundingBox DirtyBounds;
	for (AActor* Actor : DirtyActors)
	{
		if (!Actor)
		{
			continue;
		}

		ExpandBoundsByBox(DirtyBounds, BuildActorVisibleBounds(Actor, true));
	}

	const bool bNeedsRootGrowth =
		DirtyBounds.IsValid() &&
		!Octree->GetCellBounds().IsContains(DirtyBounds);

	if (bNeedsRootGrowth)
	{
		RebuildRootBounds(DirtyBounds);
		ClearQueuedActorFlags();
		DirtyActors.clear();
		return;
	}

	for (AActor* Actor : DirtyActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
		{
			if (!IsValid(Prim))
			{
				continue;
			}

			if (!Prim->IsVisible())
			{
				if (Prim->IsInOctreeOverflow())
				{
					RemovePrimitive(Prim);
				}
				else if (Prim->GetOctreeNode())
				{
					// cached OctreeNode 가 stale (freed FOctree) 일 수 있으니 root traversal
					// 로 안전하게 제거. 실패해도 OctreeLocation 은 어차피 stale 일 뿐 다음
					// Insert 가 새 node 로 덮어씀.
					Octree->Remove(Prim);
				}
				continue;
			}

			const FBoundingBox PrimBox = Prim->GetWorldBoundingBox();

			if (Prim->IsInOctreeOverflow())
			{
				RemovePrimitive(Prim);

				if (!Octree->Insert(Prim))
				{
					InsertPrimitive(Prim);
				}
				continue;
			}

			if (Prim->GetOctreeNode())
			{
				// cached node 의 LooseBounds 도 신뢰 못 함 (freed 일 수도). Octree->Remove 로
				// root 부터 walking 해 제거 후 재삽입 — 약간 느려지지만 dangling FOctree* 가
				// 끼치는 use-after-free 를 차단.
				Octree->Remove(Prim);

				if (!Octree->Insert(Prim))
				{
					InsertPrimitive(Prim);
				}
				continue;
			}

			if (!Octree->Insert(Prim))
			{
				InsertPrimitive(Prim);
			}
		}
	}

	Octree->TryMergeRecursive();

	ClearQueuedActorFlags();
	DirtyActors.clear();
}

void FSpatialPartition::Reset(const FBoundingBox& RootBounds)
{
	ClearQueuedActorFlags();

	if (Octree)
	{
		TArray<UPrimitiveComponent*> ExistingPrimitives;
		Octree->GetAllPrimitives(ExistingPrimitives);
		for (UPrimitiveComponent* Prim : ExistingPrimitives)
		{
			if (Prim)
			{
				Prim->ClearOctreeLocation();
			}
		}
	}

	for (UPrimitiveComponent* Prim : OverflowPrimitives)
	{
		if (Prim)
		{
			Prim->ClearOctreeLocation();
		}
	}

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

		if (!Octree->Insert(Prim))
		{
			InsertPrimitive(Prim);
		}
	}
}

void FSpatialPartition::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	Actor->SetQueuedForPartitionUpdate(false);
	auto DirtyIt = std::find(DirtyActors.begin(), DirtyActors.end(), Actor);
	if (DirtyIt != DirtyActors.end())
	{
		*DirtyIt = DirtyActors.back();
		DirtyActors.pop_back();
	}

	for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
	{
		if (!Prim) continue;

		RemoveSinglePrimitive(Prim);
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

	if (!Actor->IsQueuedForPartitionUpdate())
	{
		DirtyActors.push_back(Actor);
		Actor->SetQueuedForPartitionUpdate(true);
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
				if (!Proxy->HasProxyFlag(EPrimitiveProxyFlags::NeverCull))
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
		Primitive->SetOctreeLocation(nullptr, true);
	}
}

void FSpatialPartition::RemoveSinglePrimitive(UPrimitiveComponent* Primitive)
{
	if (!IsValid(Primitive)) return;

	if (Primitive->IsInOctreeOverflow())
	{
		RemovePrimitive(Primitive);
	}
	else if (Octree)
	{
		// cached OctreeNode 가 stale 일 수 있으니 root 부터 traversal. 약간 느려지지만
		// dangling FOctree 노드 접근으로 인한 use-after-free 사고 방지. 내부적으로
		// FOctree::Remove → RemoveDirect 호출하며, RemoveDirect 는 bTryMergeNow=true 가
		// default 라 즉시 merge 시도. 하지만 root traversal 자체는 valid 트리 안에서만
		// 일어나므로 안전.
		Octree->Remove(Primitive);
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

	if (Primitive->IsInOctreeOverflow())
	{
		Primitive->ClearOctreeLocation();
	}
}
