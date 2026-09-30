// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Debug/DebugPrimitiveQueue.h"

#include "Math/MathUtils.h"

#include <cmath>

namespace
{
constexpr float kPi    = 3.14159265f;
constexpr float kTwoPi = 2.0f * kPi;

void AddCircle(FDebugPrimitiveQueue& Queue, const FVector& Center,
               const FVector& AxisX, const FVector& AxisY,
               float Radius, const FColor& Color, float Duration, int32 Segments)
{
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float Angle0 = kTwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
        const float Angle1 = kTwoPi * static_cast<float>(Index + 1) / static_cast<float>(Segments);

        const FVector P0 = Center + AxisX * (cosf(Angle0) * Radius) + AxisY * (sinf(Angle0) * Radius);
        const FVector P1 = Center + AxisX * (cosf(Angle1) * Radius) + AxisY * (sinf(Angle1) * Radius);
        Queue.AddLine(P0, P1, Color, Duration);
    }
}
} // namespace

void FDebugPrimitiveQueue::AddLine(const FVector& Start, const FVector& End,
                                   const FColor& Color, float Duration)
{
    AddLineInternal(Start, End, Color, Duration);
}

void FDebugPrimitiveQueue::AddBox(const FVector& Center, const FVector& Extent,
                                  const FColor& Color, float Duration)
{
    FVector Vertices[8];
    Vertices[0] = FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z);
    Vertices[1] = FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z);
    Vertices[2] = FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z);
    Vertices[3] = FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z);
    Vertices[4] = FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z);
    Vertices[5] = FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z);
    Vertices[6] = FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z);
    Vertices[7] = FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z);

    AddBox(Vertices[0], Vertices[1], Vertices[2], Vertices[3],
           Vertices[4], Vertices[5], Vertices[6], Vertices[7], Color, Duration);
}

void FDebugPrimitiveQueue::AddBox(const FVector& P0, const FVector& P1,
                                  const FVector& P2, const FVector& P3,
                                  const FColor& Color, float Duration)
{
    AddLineInternal(P0, P1, Color, Duration);
    AddLineInternal(P1, P2, Color, Duration);
    AddLineInternal(P2, P3, Color, Duration);
    AddLineInternal(P3, P0, Color, Duration);
}

void FDebugPrimitiveQueue::AddBox(const FVector& P0, const FVector& P1,
                                  const FVector& P2, const FVector& P3,
                                  const FVector& P4, const FVector& P5,
                                  const FVector& P6, const FVector& P7,
                                  const FColor& Color, float Duration)
{
    AddBox(P0, P1, P2, P3, Color, Duration);
    AddBox(P4, P5, P6, P7, Color, Duration);
    AddLineInternal(P0, P4, Color, Duration);
    AddLineInternal(P1, P5, Color, Duration);
    AddLineInternal(P2, P6, Color, Duration);
    AddLineInternal(P3, P7, Color, Duration);
}

void FDebugPrimitiveQueue::AddSphere(const FVector& Center, float Radius, int32 Segments,
                                     const FColor& Color, float Duration)
{
    if (Segments < 4)
    {
        Segments = 4;
    }

    AddCircle(*this, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), Radius, Color, Duration, Segments);
    AddCircle(*this, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Color, Duration, Segments);
    AddCircle(*this, Center, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Color, Duration, Segments);
}

void FDebugPrimitiveQueue::AddArrow(const FVector& Start, const FVector& Direction,
                                    float Length, const FColor& Color, float Duration,
                                    int32 Segments)
{
    const FVector NormalizedDirection = Direction.Normalized();
    if (Length <= FMath::Epsilon || NormalizedDirection.LengthSquared() <= FMath::Epsilon)
    {
        return;
    }

    const float   StemLength = Length * 0.8f;
    const float   StemRadius = Length * 0.04f;
    const float   HeadRadius = Length * 0.1f;
    const FVector Tip        = Start + NormalizedDirection * Length;
    const FVector StemEnd    = Start + NormalizedDirection * StemLength;

    FVector WorldUp(0.0f, 0.0f, 1.0f);
    if (fabsf(NormalizedDirection.Dot(WorldUp)) > 0.98f)
    {
        WorldUp = FVector(1.0f, 0.0f, 0.0f);
    }

    const FVector AxisX = NormalizedDirection.Cross(WorldUp).Normalized();
    const FVector AxisY = NormalizedDirection.Cross(AxisX).Normalized();

    AddCircle(*this, Start, AxisX, AxisY, StemRadius, Color, Duration, Segments);
    AddCircle(*this, StemEnd, AxisX, AxisY, StemRadius, Color, Duration, Segments);
    AddCircle(*this, StemEnd, AxisX, AxisY, HeadRadius, Color, Duration, Segments);

    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float   Angle      = kTwoPi * static_cast<float>(Index) / 4.0f;
        const FVector StemOffset = AxisX * (cosf(Angle) * StemRadius) + AxisY * (sinf(Angle) * StemRadius);
        const FVector HeadOffset = AxisX * (cosf(Angle) * HeadRadius) + AxisY * (sinf(Angle) * HeadRadius);

        AddLineInternal(Start + StemOffset, StemEnd + StemOffset, Color, Duration);
        AddLineInternal(StemEnd + HeadOffset, Tip, Color, Duration);
    }
}

void FDebugPrimitiveQueue::Tick(float DeltaTime)
{
    for (int32 Index = static_cast<int32>(PersistentEntries.size()) - 1; Index >= 0; --Index)
    {
        FPersistentDebugLine& Entry = PersistentEntries[Index];
        Entry.RemainingTime -= DeltaTime;
        if (Entry.RemainingTime <= 0.0f)
        {
            PersistentEntries.erase(PersistentEntries.begin() + Index);
            PersistentLines.erase(PersistentLines.begin() + Index);
        }
    }
}

void FDebugPrimitiveQueue::Clear()
{
    OneFrameLines.clear();
    PersistentLines.clear();
    PersistentEntries.clear();
}

void FDebugPrimitiveQueue::AddLineInternal(const FVector& Start, const FVector& End,
                                           const FColor& Color, float Duration)
{
    const FDebugLineItem Line = { Start, End, Color };

    if (Duration > 0.0f)
    {
        PersistentLines.push_back(Line);
        PersistentEntries.push_back({ Line, Duration });
        return;
    }

    OneFrameLines.push_back(Line);
}
