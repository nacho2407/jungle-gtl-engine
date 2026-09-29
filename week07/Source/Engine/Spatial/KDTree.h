#pragma once

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Geometry/AABB.h"
#include "Engine/Geometry/Ray.h"
#include "Render/Resource/VertexTypes.h"

struct FKDNode
{
    FAABB          Bounds;
    FKDNode*       Left = nullptr;
    FKDNode*       Right = nullptr;

	int32 SplitAxis = -1;
    float SplitPos = 0.0f;

    TArray<uint32> TriangleIndices;

    bool IsLeaf() const { return Left == nullptr && Right == nullptr; }
};

struct SAHCandidate
{
    float Position;
    int   Type;
};

class FKDTree
{
  public:
    void     Build(const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices);
    FKDNode* BuildRecursive(const FAABB& CurrentBounds, TArray<uint32>& TriangleIndices, uint32 Depth);
    float    FindBestSplit(const FAABB& Bounds, const TArray<uint32>& TriangleIndices,
                           int& OutAxis, float& OutPos);
    FAABB CalculateTriangleAABB(const TArray<FNormalVertex>& Vertices,
                                const TArray<uint32>& Indices, uint32 TriIdx);

	bool RayCast(const FRay& Ray, const TArray<FNormalVertex>& Verts, const TArray<uint32>& Idxs,
                 float& OutDist) const;

  private:
      static inline bool IntersectRayTriangle(const FRay& Ray, const TArray<FNormalVertex>& Vertices,
                                                 const TArray<uint32>& Indices, uint32 TriangleIdx,
                                                 float& OutT);

  private:
    FKDNode*      Root = nullptr;
    TArray<FAABB> TriangleAABBs;
    int32 MaxLeafSize = 2;
    uint32 MaxDepth = 20;
};



inline bool FKDTree::IntersectRayTriangle(const FRay& Ray, const TArray<FNormalVertex>& Vertices,
                                   const TArray<uint32>& Indices, uint32 TriangleIdx, float& OutT)
{
    // 1. 삼각형의 세 정점 가져오기
    const FVector& V0 = Vertices[Indices[TriangleIdx * 3 + 0]].Position;
    const FVector& V1 = Vertices[Indices[TriangleIdx * 3 + 1]].Position;
    const FVector& V2 = Vertices[Indices[TriangleIdx * 3 + 2]].Position;

    // 2. 에지 벡터 계산
    const FVector Edge1 = V1 - V0;
    const FVector Edge2 = V2 - V0;

    // 3. 행렬식(Determinant) 계산
    const FVector PVec = FVector::CrossProduct(Ray.Direction, Edge2);
    const float   Det = FVector::DotProduct(Edge1, PVec);

    // Det가 0에 가깝다면 레이와 삼각형이 평행함 (Epsilon 처리)
    if (Det > -1e-6f && Det < 1e-6f)
        return false;

    const float InvDet = 1.0f / Det;

    // 4. V0에서 레이 시작점까지의 벡터 (T-Vector)
    const FVector TVec = Ray.Origin - V0;

    // 5. Barycentric Coordinate 'U' 계산
    const float U = FVector::DotProduct(TVec, PVec) * InvDet;
    if (U < 0.0f || U > 1.0f)
        return false;

    // 6. Barycentric Coordinate 'V' 계산
    const FVector QVec = FVector::CrossProduct(TVec, Edge1);
    const float   V = FVector::DotProduct(Ray.Direction, QVec) * InvDet;
    if (V < 0.0f || U + V > 1.0f)
        return false;

    // 7. 레이의 거리 'T' 계산
    const float T = FVector::DotProduct(Edge2, QVec) * InvDet;

    // T가 0보다 커야 레이 진행 방향임
    if (T > 1e-4f)
    {
        OutT = T;
        return true;
    }

    return false;
}