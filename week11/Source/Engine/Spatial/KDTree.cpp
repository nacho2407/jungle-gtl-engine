#include "KDTree.h"

FAABB FKDTree::CalculateTriangleAABB(const TArray<FNormalVertex>& Vertices,
                                     const TArray<uint32>& Indices, uint32 TriIdx)
{
    const FVector& V0 = Vertices[Indices[TriIdx * 3 + 0]].Position;
    const FVector& V1 = Vertices[Indices[TriIdx * 3 + 1]].Position;
    const FVector& V2 = Vertices[Indices[TriIdx * 3 + 2]].Position;

    FAABB TriAABB;
    TriAABB.Expand(V0);
    TriAABB.Expand(V1);
    TriAABB.Expand(V2);
    return TriAABB;
}

void FKDTree::Build(const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
    Root = nullptr;

	uint32 TriangleCount = static_cast<int32>(Indices.size() / 3);
    if (TriangleCount <= 0)
    {
        return;
    }

    TriangleAABBs.reserve(TriangleCount);
    TArray<uint32> InitialIndices;
    InitialIndices.reserve(TriangleCount);

    FAABB SceneBounds;
    for (uint32 i = 0; i < TriangleCount; ++i)
    {
        FAABB TriAABB = CalculateTriangleAABB(Vertices, Indices, i);

        TriangleAABBs.push_back(TriAABB);
        InitialIndices.push_back(i);

        SceneBounds.Expand(TriAABB.Min);
        SceneBounds.Expand(TriAABB.Max);
    }

    Root = BuildRecursive(SceneBounds, InitialIndices, 0);
}

FKDNode* FKDTree::BuildRecursive(const FAABB& CurrentBounds, TArray<uint32>& TriangleIndices,
                                 uint32 Depth)
{
    if (TriangleIndices.size() == 0)
        return nullptr;

    FKDNode* Node = new FKDNode();
    Node->Bounds = CurrentBounds;

    if (TriangleIndices.size() <= MaxLeafSize || Depth >= MaxDepth)
    {
        Node->TriangleIndices = TriangleIndices;
        return Node;
    }

    int32 BestAxis = -1;
    float SplitPos = 0.0f;
    float BestCost = FindBestSplit(CurrentBounds, TriangleIndices, BestAxis, SplitPos);

    if (BestAxis == -1 || BestCost > TriangleIndices.size())
    {
        Node->TriangleIndices = TriangleIndices;
        return Node;
    }

    Node->SplitAxis = BestAxis;
    Node->SplitPos = SplitPos;

    FAABB LeftBounds = CurrentBounds;
    LeftBounds.Max[BestAxis] = SplitPos;

    FAABB RightBounds = CurrentBounds;
    RightBounds.Min[BestAxis] = SplitPos;

    TArray<uint32> LeftIndices, RightIndices;

    for (uint32 TriIdx : TriangleIndices)
    {
        if (TriangleAABBs[TriIdx].Min[BestAxis] <= SplitPos)
            LeftIndices.push_back(TriIdx);
        else if (TriangleAABBs[TriIdx].Max[BestAxis] >= SplitPos)
            RightIndices.push_back(TriIdx);
        else
        {
            if (SplitPos - TriangleAABBs[TriIdx].Min[BestAxis] <
                TriangleAABBs[TriIdx].Max[BestAxis] - SplitPos)
            {
                LeftIndices.push_back(TriIdx);
            }
            else
            {
                RightIndices.push_back(TriIdx);
            }
        }
    }

    Node->Left = BuildRecursive(LeftBounds, LeftIndices, Depth + 1);
    Node->Right = BuildRecursive(RightBounds, RightIndices, Depth + 1);

    return Node;
}

float FKDTree::FindBestSplit(const FAABB& Bounds, const TArray<uint32>& TriangleIndices,
                             int& OutAxis, float& OutPos)
{
    float   BestCost = FLT_MAX;
    FVector NodeSize = Bounds.Max - Bounds.Min;

    float AreaTotal =
        2.0f * (NodeSize.X * NodeSize.Y + NodeSize.Y * NodeSize.Z + NodeSize.Z * NodeSize.X);

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        // 후보 지점 선정 (여기서는 간단히 8등분 지점이나 삼각형들의 경계 사용)
        for (uint32 Idx : TriangleIndices)
        {
            // Min, Max 두 군데 모두 후보가 될 수 있음
            float Candidates[2] = {TriangleAABBs[Idx].Min[Axis], TriangleAABBs[Idx].Max[Axis]};

            for (float TestPos : Candidates)
            {
                if (TestPos <= Bounds.Min[Axis] || TestPos >= Bounds.Max[Axis])
                    continue;

                // 3. 자식 노드의 표면적 계산
                FVector LeftSize = NodeSize;
                LeftSize[Axis] = TestPos - Bounds.Min[Axis];
                float AreaL = 2.0f * (LeftSize.X * LeftSize.Y + LeftSize.Y * LeftSize.Z +
                                      LeftSize.Z * LeftSize.X);

                FVector RightSize = NodeSize;
                RightSize[Axis] = Bounds.Max[Axis] - TestPos;
                float AreaR = 2.0f * (RightSize.X * RightSize.Y + RightSize.Y * RightSize.Z +
                                      RightSize.Z * RightSize.X);

                // 4. 삼각형 개수 파악
                int LeftCount = 0, RightCount = 0;
                for (uint32 T : TriangleIndices)
                {
                    if (TriangleAABBs[T].Min[Axis] < TestPos)
                        LeftCount++;
                    if (TriangleAABBs[T].Max[Axis] > TestPos)
                        RightCount++;
                }

                // 5. SAH 비용 계산: Cost = C_traverse + (Prob_L * Count_L + Prob_R * Count_R) *
                // C_intersect 확률 Prob = Area_자식 / Area_부모
                float Cost =
                    1.0f + (AreaL / AreaTotal * LeftCount + AreaR / AreaTotal * RightCount);

                if (Cost < BestCost)
                {
                    BestCost = Cost;
                    OutPos = TestPos;
                    OutAxis = Axis; // 반드시 저장해야 함!
                }
            }
        }
    }
    return BestCost;
}

bool FKDTree::RayCast(const FRay& Ray, const TArray<FNormalVertex>& Verts,
                      const TArray<uint32>& Idxs, float& OutDist) const
{
    if (!Root)
    {
        return false;
    }

    float TMin, TMax;
    if (!Root->Bounds.IntersectRay(Ray, TMin, TMax))
    {
        return false;
    }

    // 2. 비재귀 탐색용 고정 크기 스택 (힙 할당 방지)
    struct FStackNode
    {
        FKDNode* Node;
        float    TMin, TMax;
    };
    FStackNode Stack[64];
    int32      StackPtr = 0;
    Stack[StackPtr++] = {Root, TMin, TMax};

    bool bHit = false;
    OutDist = FLT_MAX;

    while (StackPtr > 0)
    {
        FStackNode Cur = Stack[--StackPtr];

        if (Cur.TMin > OutDist)
        {
            continue;
        }

        if (Cur.Node->IsLeaf())
        {
            // 리프 노드 내부 삼각형 검사
            for (uint32 TriIdx : Cur.Node->TriangleIndices)
            {
                float T;
                // 실제 삼각형 교차 판정 (Möller-Trumbore 등)
                if (IntersectRayTriangle(Ray, Verts, Idxs, TriIdx, T) && T > 0.0f)
                {
                    if (T < OutDist)
                    {
                        OutDist = T;
                        bHit = true;
                    }
                }
            }
            if (bHit && OutDist <= Cur.TMax)
            {
                return true;
            }
        }
        else // 브랜치 노드 (내부 노드)
        {
            // 1. 분할 정보 가져오기
            const int32 Axis = Cur.Node->SplitAxis;
            const float SplitPos = Cur.Node->SplitPos;

            // 2. 레이 시작점부터 분할 평면(SplitPos)까지의 거리 계산
            // Ray.Origin + Ray.Direction * T = SplitPos  => T = (SplitPos - Origin) / Dir
            float TSplit = (SplitPos - Ray.Origin[Axis]) / Ray.Direction[Axis];

            // 3. 레이 진행 방향에 따라 가까운 자식(Near)과 먼 자식(Far) 결정
            FKDNode* NearChild = (Ray.Origin[Axis] < SplitPos) ? Cur.Node->Left : Cur.Node->Right;
            FKDNode* FarChild = (NearChild == Cur.Node->Left) ? Cur.Node->Right : Cur.Node->Left;

            // 4. 스택 푸시 전략 (TSplit 값에 따른 분기)
            if (TSplit > Cur.TMax || TSplit <= 0.0f)
            {
                // 분할 평면이 현재 노드 범위(TMin~TMax)보다 뒤에 있거나,
                // 레이의 진행 방향과 반대에 있다면 NearChild만 방문
                if (NearChild)
                    Stack[StackPtr++] = {NearChild, Cur.TMin, Cur.TMax};
            }
            else if (TSplit < Cur.TMin)
            {
                // 분할 평면이 레이 시작점보다 앞에 있다면 FarChild만 방문
                if (FarChild)
                    Stack[StackPtr++] = {FarChild, Cur.TMin, Cur.TMax};
            }
            else
            {
                // 양쪽 노드 모두 레이가 관통하는 경우
                // 먼 쪽(Far)을 먼저 넣고(나중에 탐색), 가까운 쪽(Near)을 나중에 넣음(먼저 탐색)
                if (FarChild)
                    Stack[StackPtr++] = {FarChild, TSplit, Cur.TMax};
                if (NearChild)
                    Stack[StackPtr++] = {NearChild, Cur.TMin, TSplit};
            }
        }
    }
    return bHit;
}
