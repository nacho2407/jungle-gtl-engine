#include "Octree.h"
#include <Collision/RayUtils.h>
#include <algorithm>
#include <UI/EditorConsoleWidget.h>

FOctree::FOctree() 
    : BoundOctree(), Depth(0)
{
}

FOctree::FOctree(const FBoundingBox& BoundOctree, const uint32& depth)
	: BoundOctree(BoundOctree), Depth(depth)
{
}

FOctree::~FOctree()
{
	PrimitiveList.clear();

    for (FOctree* Child : Children)
    {
        delete Child;
    }
    Children.clear();
}

bool FOctree::Insert(UPrimitiveComponent* primitivie)
{
	if(!primitivie) return false;
	
	FBoundingBox PrimBox = primitivie->GetWorldBoundingBox();
	//사이즈 체크 -> primitive가 안에 들어왔는지 확인한다
	if(!BoundOctree.IsIntersected(PrimBox)) return false;
 
	// ── 내부 노드: 자식에 완전히 들어가면 위임, 아니면 이 노드에 보관 ──
	if (!IsLeaf())
	{
		for (FOctree* Child : Children)
		{
			if (Child && Child->GetBounds().IsContains(PrimBox))
				return Child->Insert(primitivie);
		}
		// 경계에 걸친 객체 → 이 노드에 보관 (재분할 없음)
		PrimitiveList.push_back(primitivie);
		return true;
	}
 
	// ── 리프 노드 ──
	PrimitiveList.push_back(primitivie);
	 if (!bSubdivideLocked
        && (int)PrimitiveList.size() > MAX_SIZE
        && Depth < MAX_DEPTH)
	 {
        SubDivide();
	 }
 
	// 용량 초과 && 깊이 여유 있음 && 실제로 분배 가능한 객체가 존재할 때만 분할
	/*if ((int)PrimitiveList.size() > MAX_SIZE
		&& Depth < MAX_DEPTH
		&& HasDistributable())
	{
		SubDivide();
	}*/
 
	return true;
}

bool FOctree::Remove(const UPrimitiveComponent* Primitive)
{
	if(Primitive == nullptr) return false;

	auto It = std::find(PrimitiveList.begin(), PrimitiveList.end(), Primitive);
    if (It != PrimitiveList.end())
    {
        *It = PrimitiveList.back();
        PrimitiveList.pop_back();

        if (!IsLeaf()) TryMerge();
        return true;
    }

    if (IsLeaf()) return false;

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i]->Remove(Primitive))
        {
            TryMerge();
			bSubdivideLocked = false;
            return true;
        }
    }

	return false;
}

void FOctree::TryMerge()
{
	if(IsLeaf()) return;

	for (int Index = 0; Index < 8; ++Index)
	{
		if (!Children[Index]->IsLeaf())
		{
			return; // 하나라도 리프가 아니면 합치지 않음
		}
	}
	
	uint32 TotalPrimitives = PrimitiveList.size();
	for (int Index = 0; Index < 8; ++Index)
	{
		TotalPrimitives += Children[Index]->PrimitiveList.size();
	}

    if (TotalPrimitives <= MAX_SIZE)
    {
        for (FOctree* Child : Children)
        {
            PrimitiveList.insert(PrimitiveList.end(),
                Child->PrimitiveList.begin(),
                Child->PrimitiveList.end());
            delete Child;
        }
        Children.clear();
    }
}

void FOctree::SubDivide()
{ 
	if (!IsLeaf())
        return;

    const FVector Center = BoundOctree.GetCenter();
    const FVector Min = BoundOctree.Min;
    const FVector Max = BoundOctree.Max;
	
	const FBoundingBox ChildBoxes[8] = {
        { FVector(Min.X,    Center.Y, Min.Z),    FVector(Center.X, Max.Y,    Center.Z) },
        { FVector(Center.X, Center.Y, Min.Z),    FVector(Max.X,    Max.Y,    Center.Z) },
        { FVector(Min.X,    Center.Y, Center.Z), FVector(Center.X, Max.Y,    Max.Z)    },
        { FVector(Center.X, Center.Y, Center.Z), FVector(Max.X,    Max.Y,    Max.Z)    },
        { FVector(Min.X,    Min.Y,    Min.Z),    FVector(Center.X, Center.Y, Center.Z) },
        { FVector(Center.X, Min.Y,    Min.Z),    FVector(Max.X,    Center.Y, Center.Z) },
        { FVector(Min.X,    Min.Y,    Center.Z), FVector(Center.X, Center.Y, Max.Z)    },
        { FVector(Center.X, Min.Y,    Center.Z), FVector(Max.X,    Center.Y, Max.Z)    },
    };

    Children.resize(8, nullptr);
    for (int i = 0; i < 8; ++i)
        Children[i] = new FOctree(ChildBoxes[i], Depth + 1);

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
        for (int i = 0; i < 8; ++i)
        {
            if (ChildBoxes[i].IsContains(PrimBox))
            {
                Children[i]->Insert(Prim);
                bPlaced = true;
                ++Distributed;
                break;
            }
        }
        if (!bPlaced)
            PrimitiveList.push_back(Prim);
    }

	if (Distributed == 0)
    {
        for (FOctree* Child : Children) delete Child;
        Children.clear();
        // PrimitiveList는 이미 위에서 크로스-바운더리로 재구성됨
        bSubdivideLocked = true; // ← 이후 Insert에서 검사 자체를 건너뜀
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
            return A->GetWorldBoundingBox().GetCenterDistanceSquared(Pos)
                 < B->GetWorldBoundingBox().GetCenterDistanceSquared(Pos);
        });

    if (Candidates.size() > Count)
    {
        Candidates.resize(Count);
    }

    return Candidates;
}

void FOctree::QueryAABB(const FBoundingBox& QueryBox, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
    if (!BoundOctree.IsIntersected(QueryBox))
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
	if (!FRayUtils::CheckRayAABB(Ray, BoundOctree.Min, BoundOctree.Max))
        return;

    for (UPrimitiveComponent* Primitive : PrimitiveList)
    {
		const FBoundingBox & box = Primitive->GetWorldBoundingBox();
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
	 for (FOctree* Child : Children)
    {
        delete Child;
    }
    Children.clear();

    PrimitiveList.clear();
    BoundOctree = InBounds;
    Depth = InDepth;
}

bool FOctree::HasDistributable() const
{
	const FVector Center = BoundOctree.GetCenter();
    const FVector Min    = BoundOctree.Min;
    const FVector Max    = BoundOctree.Max;

    const FBoundingBox ChildBoxes[8] = {
        { FVector(Min.X,    Center.Y, Min.Z),    FVector(Center.X, Max.Y,    Center.Z) },
        { FVector(Center.X, Center.Y, Min.Z),    FVector(Max.X,    Max.Y,    Center.Z) },
        { FVector(Min.X,    Center.Y, Center.Z), FVector(Center.X, Max.Y,    Max.Z)    },
        { FVector(Center.X, Center.Y, Center.Z), FVector(Max.X,    Max.Y,    Max.Z)    },
        { FVector(Min.X,    Min.Y,    Min.Z),    FVector(Center.X, Center.Y, Center.Z) },
        { FVector(Center.X, Min.Y,    Min.Z),    FVector(Max.X,    Center.Y, Center.Z) },
        { FVector(Min.X,    Min.Y,    Center.Z), FVector(Center.X, Center.Y, Max.Z)    },
        { FVector(Center.X, Min.Y,    Center.Z), FVector(Max.X,    Center.Y, Max.Z)    },
    };

    for (UPrimitiveComponent* Prim : PrimitiveList)
    {
        if (!Prim) continue;
        const FBoundingBox PrimBox = Prim->GetWorldBoundingBox();
        for (const FBoundingBox& ChildBox : ChildBoxes)
        {
            if (ChildBox.IsContains(PrimBox))
                return true; // ← 하나만 찾으면 즉시 종료
        }
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

	if (IsLeaf()) return;

	for (int i = 0; i < 8; ++i)
		Children[i]->CollectAll(OutPrimitives);
}

void FOctree::QueryFrustumInternal(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives, bool bParentContained) const
{
	if (!bParentContained)
	{
		// Node might be outside — test intersection first
		if (!ConvexVolume.IntersectAABB(BoundOctree))
			return;

		// Node fully inside frustum — skip per-primitive tests for entire subtree
		if (ConvexVolume.ContainsAABB(BoundOctree))
		{
			CollectAll(OutPrimitives);
			return;
		}
	}

	// Partial overlap (or parent was contained but we still need visibility check)
	if (bParentContained)
	{
		// Parent fully contained — all primitives here are inside frustum
		for (UPrimitiveComponent* Primitive : PrimitiveList)
		{
			if (Primitive)
				OutPrimitives.push_back(Primitive);
		}
	}
	else
	{
		// Partial overlap — test each primitive individually
		for (UPrimitiveComponent* Primitive : PrimitiveList)
		{
			if (Primitive && ConvexVolume.IntersectAABB(Primitive->GetWorldBoundingBox()))
				OutPrimitives.push_back(Primitive);
		}
	}

	if (IsLeaf()) return;

	for (int i = 0; i < 8; ++i)
		Children[i]->QueryFrustumInternal(ConvexVolume, OutPrimitives, bParentContained);
}

// ================================================================
// Proxy-direct frustum query — avoids Component→GetSceneProxy() indirection
// ================================================================

void FOctree::QueryFrustumProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies) const
{
	QueryFrustumProxiesInternal(ConvexVolume, OutProxies, false);
}

void FOctree::CollectAllProxies(TArray<FPrimitiveSceneProxy*>& OutProxies) const
{
	for (UPrimitiveComponent* Primitive : PrimitiveList)
	{
		if (Primitive)
		{
			if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
				OutProxies.push_back(Proxy);
		}
	}

	if (IsLeaf()) return;

	for (int i = 0; i < 8; ++i)
		Children[i]->CollectAllProxies(OutProxies);
}

void FOctree::QueryFrustumProxiesInternal(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies, bool bParentContained) const
{
	if (!bParentContained)
	{
		if (!ConvexVolume.IntersectAABB(BoundOctree))
			return;

		if (ConvexVolume.ContainsAABB(BoundOctree))
		{
			CollectAllProxies(OutProxies);
			return;
		}
	}

	if (bParentContained)
	{
		for (UPrimitiveComponent* Primitive : PrimitiveList)
		{
			if (Primitive)
			{
				if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
					OutProxies.push_back(Proxy);
			}
		}
	}
	else
	{
		for (UPrimitiveComponent* Primitive : PrimitiveList)
		{
			if (Primitive && ConvexVolume.IntersectAABB(Primitive->GetWorldBoundingBox()))
			{
				if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
					OutProxies.push_back(Proxy);
			}
		}
	}

	if (IsLeaf()) return;

	for (int i = 0; i < 8; ++i)
		Children[i]->QueryFrustumProxiesInternal(ConvexVolume, OutProxies, bParentContained);
}
