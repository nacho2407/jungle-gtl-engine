#pragma once

#include "Viewport/ViewportTypes.h"

class AActor;
class UScene;
class CCamera;
class UWorld;

struct FRay
{
    FVector Origin;
    FVector Direction;
};

class CPicker
{
public:
    // 스크린 좌표 → 월드 레이 변환 (Deprojection)
    FRay ScreenToRay(const CCamera* Camera, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight) const;

	FRay ScreenToRay(const FViewportEntry& Entry, int32 ScreenX, int32 ScreenY) const;

    // Möller–Trumbore 알고리즘: 레이-삼각형 교차 검사
    bool RayTriangleIntersect(const FRay& Ray,
                              const FVector& V0, const FVector& V1, const FVector& V2,
                              float& OutDistance) const;

    // 씬의 모든 Actor를 대상으로 피킹 (가장 가까운 Actor 반환)
    AActor* PickActor(UScene* Scene, int32 ScreenX, int32 ScreenY,
                      int32 ScreenWidth, int32 ScreenHeight) const;

	AActor* PickActor(UWorld* World, const FViewportEntry& Entry, int32 ScreenX, int32 ScreenY) const;
};
