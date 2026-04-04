#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Engine/Math/ConvexVolume.h"
#include "Engine/Component/CameraComponent.h"
#include <algorithm>

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::~UWorld()
{
	if (!Actors.empty())
	{
		EndPlay();
	}

	delete Octree;
}

void UWorld::DestroyActor(AActor* Actor)
{
	// remove and clean up
	if (!Actor) return;
	Actor->EndPlay();
	// Remove from actor list
	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
		Actors.erase(it);

	MarkPickingBVHDirty();

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	Actor->SetWorld(this);
	Actors.push_back(Actor);
	MarkPickingBVHDirty();
}

void UWorld::MarkPickingBVHDirty()
{
	bPickingBVHDirty = true;
}

void UWorld::BuildPickingBVHNow() const
{
	RebuildPickingBVH();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	EnsurePickingBVH();

	OutHitResult = {};
	OutActor = nullptr;
	if (PickingNodes.empty())
	{
		return false;
	}

	TArray<int32> NodeStack;
	NodeStack.push_back(0);

	while (!NodeStack.empty())
	{
		const int32 NodeIndex = NodeStack.back();
		NodeStack.pop_back();

		const FPickingBVHNode& Node = PickingNodes[NodeIndex];
		if (!FRayUtils::CheckRayAABB(Ray, Node.Bounds.Min, Node.Bounds.Max))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			for (int32 LeafIndex = Node.FirstLeaf; LeafIndex < Node.FirstLeaf + Node.LeafCount; ++LeafIndex)
			{
				const FPickingBVHLeaf& Leaf = PickingLeaves[LeafIndex];
				if (!Leaf.Primitive || !Leaf.Owner || !Leaf.Owner->IsVisible() || !Leaf.Primitive->IsVisible())
				{
					continue;
				}

				FHitResult CandidateHit{};
				if (FRayUtils::RaycastComponent(Leaf.Primitive, Ray, CandidateHit) &&
					CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
					OutActor = Leaf.Owner;
				}
			}
			continue;
		}

		if (Node.LeftChild >= 0)
		{
			NodeStack.push_back(Node.LeftChild);
		}
		if (Node.RightChild >= 0)
		{
			NodeStack.push_back(Node.RightChild);
		}
	}

	return OutActor != nullptr;
}

void UWorld::EnsurePickingBVH() const
{
	if (!bPickingBVHDirty)
	{
		return;
	}

	RebuildPickingBVH();
}

void UWorld::RebuildPickingBVH() const
{
	PickingLeaves.clear();
	PickingNodes.clear();

	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible())
			{
				continue;
			}

			FPickingBVHLeaf Leaf;
			Leaf.Primitive = Primitive;
			Leaf.Owner = Actor;
			Leaf.Bounds = Primitive->GetWorldBoundingBox();

			if (!Leaf.Bounds.IsValid())
			{
				continue;
			}

			PickingLeaves.push_back(Leaf);
		}
	}

	if (!PickingLeaves.empty())
	{
		BuildPickingBVHRecursive(0, static_cast<int32>(PickingLeaves.size()));
	}

	bPickingBVHDirty = false;
}

int32 UWorld::BuildPickingBVHRecursive(int32 Start, int32 End) const
{
	const int32 NodeIndex = static_cast<int32>(PickingNodes.size());
	PickingNodes.emplace_back();

	FBoundingBox Bounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		const FBoundingBox& LeafBounds = PickingLeaves[LeafIndex].Bounds;
		Bounds.Expand(LeafBounds.Min);
		Bounds.Expand(LeafBounds.Max);
	}
	PickingNodes[NodeIndex].Bounds = Bounds;

	constexpr int32 MaxLeafSize = 8;
	const int32 LeafCount = End - Start;
	if (LeafCount <= MaxLeafSize)
	{
		PickingNodes[NodeIndex].FirstLeaf = Start;
		PickingNodes[NodeIndex].LeafCount = LeafCount;
		return NodeIndex;
	}

	FBoundingBox CentroidBounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		CentroidBounds.Expand(PickingLeaves[LeafIndex].Bounds.GetCenter());
	}

	const FVector CentroidExtent = CentroidBounds.GetExtent();
	int32 SplitAxis = 0;
	if (CentroidExtent.Y > CentroidExtent.X && CentroidExtent.Y >= CentroidExtent.Z)
	{
		SplitAxis = 1;
	}
	else if (CentroidExtent.Z > CentroidExtent.X && CentroidExtent.Z > CentroidExtent.Y)
	{
		SplitAxis = 2;
	}

	const int32 Mid = Start + LeafCount / 2;
	std::nth_element(
		PickingLeaves.begin() + Start,
		PickingLeaves.begin() + Mid,
		PickingLeaves.begin() + End,
		[SplitAxis](const FPickingBVHLeaf& A, const FPickingBVHLeaf& B)
		{
			const FVector CenterA = A.Bounds.GetCenter();
			const FVector CenterB = B.Bounds.GetCenter();
			if (SplitAxis == 1)
			{
				return CenterA.Y < CenterB.Y;
			}
			if (SplitAxis == 2)
			{
				return CenterA.Z < CenterB.Z;
			}
			return CenterA.X < CenterB.X;
		});

	PickingNodes[NodeIndex].LeftChild = BuildPickingBVHRecursive(Start, Mid);
	PickingNodes[NodeIndex].RightChild = BuildPickingBVHRecursive(Mid, End);
	return NodeIndex;
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim || !Prim->IsVisible()) continue;
        Prim->UpdateWorldMatrix();
        Octree->Insert(Prim);
    }
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim) continue;
        Octree->Remove(Prim);
    }
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
    if (!Actor) return;

    for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
    {
        if (!Prim) continue;

        Octree->Remove(Prim);
        Prim->UpdateWorldMatrix();

        if (Prim->IsVisible())
        {
            Octree->Insert(Prim);
        }
    }
}

void UWorld::UpdateVisibleActors()
{
	VisiblePrimitives.clear();
	VisibleActors.clear();

	// 모든 프록시를 컬링 상태로 초기화
	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (Proxy) Proxy->bFrustumCulled = true;
	}

	FConvexVolume ConvexVolume = ActiveCamera->GetConvexVolume();

	Octree->QueryFrustum(ConvexVolume, VisiblePrimitives);

	// 보이는 primitive의 프록시만 컬링 해제
	for (UPrimitiveComponent* Primitive : VisiblePrimitives)
	{
		if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
			Proxy->bFrustumCulled = false;

		if (AActor* Owner = Primitive->GetOwner())
		{
			VisibleActors.push_back(Owner);
		}
	}
}

void UWorld::InitWorld()
{
	delete Octree;
	Octree = new FOctree(FBoundingBox(FVector(-100, -100, -100), FVector(100, 100, 100)), 0);
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void UWorld::Tick(float DeltaTime)
{
	UpdateVisibleActors();
	for (AActor* Actor : VisibleActors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
	MarkPickingBVHDirty();
}
