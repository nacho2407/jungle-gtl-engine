#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Core/CollisionTypes.h"
#include "Geometry/Ray.h"
#include "Math/Matrix.h"

class UPrimitiveComponent;

// 레이캐스트 관련 정적 유틸리티
// 컴포넌트에 종속되지 않는 순수 수학 연산
struct FRayCollision
{
    // Ray vs AABB 교차 판정
    static bool CheckRayAABB(const FRay& Ray, const FVector& AABBMin, const FVector& AABBMax);

    // Ray vs Triangle 교차 (Möller–Trumbore)
    static bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1,
                                  const FVector& V2, float& OutT);

    // 메시 삼각형들에 대한 레이캐스트
    // Positions: 정점 위치 배열, PositionStride: 정점 구조체 바이트 크기
    static bool RaycastTriangles(const FRay& WorldRay, const FMatrix& WorldMatrix, const void* PositionData,
                                 uint32 PositionStride, const TArray<uint32>& Indices, FHitResult& OutHitResult);

    // 컴포넌트 단위 레이캐스트 (AABB 필터링 + LineTraceComponent)
    static bool RaycastComponent(UPrimitiveComponent* Component, const FRay& Ray, FHitResult& OutHitResult);
};
