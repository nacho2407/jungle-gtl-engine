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
	
	//자식노드가 없으면
	if (IsLeaf()) {

		//아직 더 자를 수 있으면 자르고 해당 값을 넣는다.
		if (PrimitiveList.size() < MAX_SIZE || Depth == MAX_DEPTH)
		{
			PrimitiveList.push_back(primitivie); // 해당 객체를 추가한다
			return true;
		}
		SubDivide();
	}// 자식노드가 있으면
		
	for (FOctree* Child : Children)
	{
		if (Child && Child->GetBounds().IsContains(PrimBox))
		{
			return Child->Insert(primitivie);
		}
	}
	PrimitiveList.push_back(primitivie);
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
		
    Children.resize(8, nullptr);

    const FVector Center = BoundOctree.GetCenter();
    const FVector Min = BoundOctree.Min;
    const FVector Max = BoundOctree.Max;
	
	
	Children[0] = new FOctree(FBoundingBox(FVector(Min.X, Center.Y, Min.Z), FVector(Center.X, Max.Y, Center.Z)), Depth + 1); // Top-Back-Left
	Children[1] = new FOctree(FBoundingBox(FVector(Center.X, Center.Y, Min.Z), FVector(Max.X, Max.Y, Center.Z)), Depth + 1); // Top-Back-Right
	Children[2] = new FOctree(FBoundingBox(FVector(Min.X, Center.Y, Center.Z), FVector(Center.X, Max.Y, Max.Z)), Depth + 1); // Top-Front-Left
	Children[3] = new FOctree(FBoundingBox(FVector(Center.X, Center.Y, Center.Z), FVector(Max.X, Max.Y, Max.Z)), Depth + 1); // Top-Front-Right
	Children[4] = new FOctree(FBoundingBox(FVector(Min.X, Min.Y, Min.Z), FVector(Center.X, Center.Y, Center.Z)), Depth + 1); // Bottom-Back-Left
	Children[5] = new FOctree(FBoundingBox(FVector(Center.X, Min.Y, Min.Z), FVector(Max.X, Center.Y, Center.Z)), Depth + 1); // Bottom-Back-Right
	Children[6] = new FOctree(FBoundingBox(FVector(Min.X, Min.Y, Center.Z), FVector(Center.X, Center.Y, Max.Z)), Depth + 1); // Bottom-Front-Left
	Children[7] = new FOctree(FBoundingBox(FVector(Center.X, Min.Y, Center.Z), FVector(Max.X, Center.Y, Max.Z)), Depth + 1); // Bottom-Front-Right

	TArray<UPrimitiveComponent*> primitivesToMove = PrimitiveList;
	PrimitiveList.clear();

	for (UPrimitiveComponent* prim : primitivesToMove)
	{
		Insert(prim);
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

void FOctree::MarkDirty(UPrimitiveComponent* Primitive)
{
	if (!Primitive) return;

    auto It = std::find(DirtyList.begin(), DirtyList.end(), Primitive);
    if (It == DirtyList.end())
    {
        DirtyList.push_back(Primitive);
    }
}

void FOctree::FlushDirty()
{
	for (UPrimitiveComponent* Primitive : DirtyList)
    {
        if (!Primitive) continue;

        Primitive->UpdateWorldMatrix();
        Remove(Primitive);
        Insert(Primitive);
    }

    DirtyList.clear();
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

void FOctree::QueryFrustum(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!ConvexVolume.IntersectAABB(BoundOctree))
		return;


	for (UPrimitiveComponent* Primitive : PrimitiveList)
	{
		if (Primitive && ConvexVolume.IntersectAABB(Primitive->GetWorldBoundingBox()))
		{
			OutPrimitives.push_back(Primitive);
		}
	}


	if (IsLeaf())
		return;

	for (int i = 0; i < 8; ++i)
	{
		Children[i]->QueryFrustum(ConvexVolume, OutPrimitives);
	}
}
