#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

struct FDebugLineItem
{
    FVector Start;
    FVector End;
    FColor  Color;
};

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
