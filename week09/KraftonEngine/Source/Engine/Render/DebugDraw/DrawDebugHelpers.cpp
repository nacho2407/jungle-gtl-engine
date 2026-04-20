#include "Render/Debug/DrawDebugHelpers.h"

#if defined(_DEBUG)

#include "GameFramework/World.h"

void DrawDebugLine(UWorld* World,
                   const FVector& Start, const FVector& End,
                   const FColor& Color, float Duration)
{
    if (!World)
        return;
    World->GetScene().GetDebugDrawQueue().AddLine(Start, End, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& Center, const FVector& Extent,
                  const FColor& Color, float Duration)
{
    if (!World)
        return;
    World->GetScene().GetDebugDrawQueue().AddBox(Center, Extent, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FColor& Color, float Duration)
{
    if (!World)
        return;
    FDebugDrawQueue& Queue = World->GetScene().GetDebugDrawQueue();
    Queue.AddLine(P0, P1, Color, Duration);
    Queue.AddLine(P1, P2, Color, Duration);
    Queue.AddLine(P2, P3, Color, Duration);
    Queue.AddLine(P3, P0, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FVector& P4, const FVector& P5,
                  const FVector& P6, const FVector& P7,
                  const FColor& Color, float Duration)
{
    if (!World)
        return;
    FDebugDrawQueue& Queue = World->GetScene().GetDebugDrawQueue();
    // 하단면
    Queue.AddLine(P0, P1, Color, Duration);
    Queue.AddLine(P1, P2, Color, Duration);
    Queue.AddLine(P2, P3, Color, Duration);
    Queue.AddLine(P3, P0, Color, Duration);
    // 상단면
    Queue.AddLine(P4, P5, Color, Duration);
    Queue.AddLine(P5, P6, Color, Duration);
    Queue.AddLine(P6, P7, Color, Duration);
    Queue.AddLine(P7, P4, Color, Duration);
    // 수직 에지
    Queue.AddLine(P0, P4, Color, Duration);
    Queue.AddLine(P1, P5, Color, Duration);
    Queue.AddLine(P2, P6, Color, Duration);
    Queue.AddLine(P3, P7, Color, Duration);
}

void DrawDebugSphere(UWorld* World,
                     const FVector& Center, float Radius,
                     int32 Segments, const FColor& Color, float Duration)
{
    if (!World)
        return;
    World->GetScene().GetDebugDrawQueue().AddSphere(Center, Radius, Segments, Color, Duration);
}

void DrawDebugPoint(UWorld* World,
                    const FVector& Position, float Size,
                    const FColor& Color, float Duration)
{
    if (!World)
        return;

    // 점을 3축 십자선으로 표현
    FVector Half(Size, Size, Size);
    World->GetScene().GetDebugDrawQueue().AddLine(
        Position - FVector(Size, 0, 0), Position + FVector(Size, 0, 0), Color, Duration);
    World->GetScene().GetDebugDrawQueue().AddLine(
        Position - FVector(0, Size, 0), Position + FVector(0, Size, 0), Color, Duration);
    World->GetScene().GetDebugDrawQueue().AddLine(
        Position - FVector(0, 0, Size), Position + FVector(0, 0, Size), Color, Duration);
}

#endif // _DEBUG
