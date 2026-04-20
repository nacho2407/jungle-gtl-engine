#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

// ============================================================
// FDebugDrawItem — 단일 디버그 라인 (Box/Sphere는 라인으로 분해하여 저장)
// ============================================================
struct FDebugDrawItem
{
    FVector Start;
    FVector End;
    FColor Color;
    float RemainingTime; // <= 0이면 이번 프레임 후 제거 (0 = 1프레임만)
    bool bOneFrame;      // Duration == 0으로 추가된 항목 (1프레임 표시 후 제거)
};

// ============================================================
// FDebugDrawQueue — Duration 기반 디버그 라인 저장소
//
// DrawDebugHelpers에서 호출 → 라인으로 분해하여 축적
// 매 프레임 Tick()으로 Duration 감소 + 만료 항목 제거
// RenderCollector가 GetItems()로 읽어서 FScene에 제출
// ============================================================
class FDebugDrawQueue
{
public:
    void AddLine(const FVector& Start, const FVector& End,
                 const FColor& Color, float Duration);

    void AddBox(const FVector& Center, const FVector& Extent,
                const FColor& Color, float Duration);

    void AddSphere(const FVector& Center, float Radius, int32 Segments,
                   const FColor& Color, float Duration);

    // Duration 감소 + 만료 항목 제거. 매 프레임 Tick 시 호출.
    void Tick(float DeltaTime);

    const TArray<FDebugDrawItem>& GetItems() const { return Items; }

    void Clear() { Items.clear(); }

private:
    TArray<FDebugDrawItem> Items;
};
