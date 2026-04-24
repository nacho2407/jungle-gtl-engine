// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

// FDebugLineItem는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FDebugLineItem
{
    FVector Start;
    FVector End;
    FColor  Color;
};

// FDebugPrimitiveQueue는 렌더 영역의 핵심 동작을 담당합니다.
class FDebugPrimitiveQueue
{
public:
    void AddLine(const FVector& Start, const FVector& End,
                 const FColor& Color, float Duration);

    void AddBox(const FVector& Center, const FVector& Extent,
                const FColor& Color, float Duration);

    void AddBox(const FVector& P0, const FVector& P1,
                const FVector& P2, const FVector& P3,
                const FColor& Color, float Duration);

    void AddBox(const FVector& P0, const FVector& P1,
                const FVector& P2, const FVector& P3,
                const FVector& P4, const FVector& P5,
                const FVector& P6, const FVector& P7,
                const FColor& Color, float Duration);

    void AddSphere(const FVector& Center, float Radius, int32 Segments,
                   const FColor& Color, float Duration);

    void AddArrow(const FVector& Start, const FVector& Direction,
                  float Length, const FColor& Color, float Duration,
                  int32 Segments = 8);

    void ClearOneFramePrimitives() { OneFrameLines.clear(); }
    void Tick(float DeltaTime);
    void Clear();

    const TArray<FDebugLineItem>& GetOneFrameLines() const { return OneFrameLines; }
    const TArray<FDebugLineItem>& GetPersistentLines() const { return PersistentLines; }

private:
    void AddLineInternal(const FVector& Start, const FVector& End,
                         const FColor& Color, float Duration);

    // FPersistentDebugLine는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FPersistentDebugLine
    {
        FDebugLineItem Line;
        float          RemainingTime = 0.0f;
    };

private:
    TArray<FDebugLineItem>       OneFrameLines;
    TArray<FDebugLineItem>       PersistentLines;
    TArray<FPersistentDebugLine> PersistentEntries;
};
