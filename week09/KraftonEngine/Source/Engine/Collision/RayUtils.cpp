// 충돌/피킹 영역의 세부 동작을 구현합니다.
#include "Collision/RayUtils.h"
#include "Component/PrimitiveComponent.h"
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <cstdint>

// AABB에 쓰이는 전형적인 slab 방식(레이가 이 AABB에 언제 들어오고 언제 나가는지)로 계산합니다.(핵심 : tMin, tMax 갱신과 early exit)
// OutTMin은 박스에 처음 들어가는 거리, OutTMax는 박스를 빠져나가는 거리입니다.
bool FRayUtils::IntersectRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax, float& OutTMin, float& OutTMax)
{
    float tMin = -INFINITY;
    float tMax = INFINITY;

    const float* origin = &Ray.Origin.X;
    const float* dir = &Ray.Direction.X;
    const float* minB = &AABBMin.X;
    const float* maxB = &AABBMax.X;

    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(dir[i]) < 1e-8f)
        {
            if (origin[i] < minB[i] || origin[i] > maxB[i])
            {
                return false;
            }
            continue;
        }

        float invDir = 1.0f / dir[i];
        float t1 = (minB[i] - origin[i]) * invDir;
        float t2 = (maxB[i] - origin[i]) * invDir;

        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
            return false;
    }

    OutTMin = tMin;
    OutTMax = tMax;
    return tMax >= 0.0f;
}

// 진입/이탈 거리 자체는 필요 없고 교차 여부만 필요할 때 쓰는 얇은 래퍼입니다.
bool FRayUtils::CheckRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax)
{
    float tMin = 0.0f;
    float tMax = 0.0f;
    return IntersectRayAABB(Ray, AABBMin, AABBMax, tMin, tMax);
}

// Moller-Trumbore 알고리즘으로 ray와 triangle의 교차를 계산합니다.
// hit 시 OutT는 ray 원점에서 삼각형까지의 양수 거리입니다.
bool FRayUtils::IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir,
                                  const FVector& V0, const FVector& V1, const FVector& V2, float& OutT)
{
    FVector edge1 = V1 - V0;
    FVector edge2 = V2 - V0;
    FVector pvec = RayDir.Cross(edge2);
    float det = edge1.Dot(pvec);

    if (std::abs(det) < 0.0001f)
        return false;

    float invDet = 1.0f / det;
    FVector tvec = RayOrigin - V0;
    float u = tvec.Dot(pvec) * invDet;

    if (u < 0.0f || u > 1.0f)
        return false;

    FVector qvec = tvec.Cross(edge1);
    float v = RayDir.Dot(qvec) * invDet;

    if (v < 0.0f || u + v > 1.0f)
        return false;

    OutT = edge2.Dot(qvec) * invDet;
    return OutT > 0.0f;
}

// BVH가 없는 fallback 경로입니다.
// world ray를 local로 변환한 뒤 모든 triangle을 순차 검사해 가장 가까운 hit를 찾습니다.
bool FRayUtils::RaycastTriangles(
    const FRay& WorldRay,
    const FMatrix& WorldMatrix,
    const FMatrix& InverseWorldMatrix,
    const void* PositionData,
    uint32 PositionStride,
    const uint32* IndexData,
    uint32 IndexCount,
    FHitResult& OutHitResult)
{
    if (!PositionData || !IndexData || IndexCount == 0)
        return false;

    FVector localOrigin = InverseWorldMatrix.TransformPositionWithW(WorldRay.Origin);
    FVector localDir = InverseWorldMatrix.TransformVector(WorldRay.Direction);
    localDir.Normalize();

    bool bHit = false;
    float closestT = FLT_MAX;
    int hitFaceIndex = -1;

    const uint8* basePtr = static_cast<const uint8*>(PositionData);

    for (uint32 i = 0; i + 2 < IndexCount; i += 3)
    {
        const FVector& v0 = *reinterpret_cast<const FVector*>(basePtr + IndexData[i] * PositionStride);
        const FVector& v1 = *reinterpret_cast<const FVector*>(basePtr + IndexData[i + 1] * PositionStride);
        const FVector& v2 = *reinterpret_cast<const FVector*>(basePtr + IndexData[i + 2] * PositionStride);

        float t = 0.0f;
        if (IntersectTriangle(localOrigin, localDir, v0, v1, v2, t))
        {
            if (t > 0.0f && t < closestT)
            {
                closestT = t;
                hitFaceIndex = static_cast<int>(i);
                bHit = true;
            }
        }
    }

    OutHitResult.bHit = bHit;
    if (bHit)
    {
        OutHitResult.FaceIndex = hitFaceIndex;
        FVector localHitPoint = localOrigin + localDir * closestT;
        FVector worldHitPoint = WorldMatrix.TransformPositionWithW(localHitPoint);
        OutHitResult.Distance = FVector::Distance(WorldRay.Origin, worldHitPoint);
    }

    return bHit;
}

bool FRayUtils::RaycastTriangles(
    const FRay& WorldRay,
    const FMatrix& WorldMatrix,
    const FMatrix& InverseWorldMatrix,
    const void* PositionData,
    uint32 PositionStride,
    const TArray<uint32>& Indices,
    FHitResult& OutHitResult)
{
    return RaycastTriangles(WorldRay, WorldMatrix, InverseWorldMatrix,
                            PositionData, PositionStride,
                            Indices.data(), (uint32)Indices.size(),
                            OutHitResult);
}

// component 단위의 공통 raycast 진입점입니다.
// visibility와 world AABB broad phase를 먼저 통과한 경우에만 실제 검사 함수를 호출합니다.
bool FRayUtils::RaycastComponent(UPrimitiveComponent* Component, const FRay& Ray, FHitResult& OutHitResult)
{
    if (!Component || !Component->IsVisible())
        return false;

    FBoundingBox AABB = Component->GetWorldBoundingBox();
    if (!CheckRayAABB(Ray, AABB.Min, AABB.Max))
        return false;

    return Component->LineTraceComponent(Ray, OutHitResult);
}
