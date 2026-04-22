#include "Render/Scene/DebugDraw/DrawDebugHelpers.h"

#if defined(_DRAW_DEBUG_ENABLE)

#include "GameFramework/World.h"
// 필요에 따라 FScene 관련 헤더를 추가해야 할 수 있습니다. 
// #include "Render/Scene/Scene.h" 

// ======================================================================
// FScene& 오버로딩 버전 (실제 드로우 로직 수행)
// ======================================================================

void DrawDebugLine(FScene& Scene,
                   const FVector& Start, const FVector& End,
                   const FColor& Color, float Duration)
{
    Scene.GetDebugDrawQueue().AddLine(Start, End, Color, Duration);
}

void DrawDebugBox(FScene& Scene,
                  const FVector& Center, const FVector& Extent,
                  const FColor& Color, float Duration)
{
    Scene.GetDebugDrawQueue().AddBox(Center, Extent, Color, Duration);
}

void DrawDebugBox(FScene& Scene,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FColor& Color, float Duration)
{
    FDebugDrawQueue& Queue = Scene.GetDebugDrawQueue();
    Queue.AddLine(P0, P1, Color, Duration);
    Queue.AddLine(P1, P2, Color, Duration);
    Queue.AddLine(P2, P3, Color, Duration);
    Queue.AddLine(P3, P0, Color, Duration);
}

void DrawDebugBox(FScene& Scene,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FVector& P4, const FVector& P5,
                  const FVector& P6, const FVector& P7,
                  const FColor& Color, float Duration)
{
    FDebugDrawQueue& Queue = Scene.GetDebugDrawQueue();
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

void DrawDebugSphere(FScene& Scene,
                     const FVector& Center, float Radius,
                     int32 Segments, const FColor& Color, float Duration)
{
    Scene.GetDebugDrawQueue().AddSphere(Center, Radius, Segments, Color, Duration);
}

void DrawDebugPoint(FScene& Scene,
                    const FVector& Position, float Size,
                    const FColor& Color, float Duration)
{
    // 점을 3축 십자선으로 표현
    Scene.GetDebugDrawQueue().AddLine(
        Position - FVector(Size, 0, 0), Position + FVector(Size, 0, 0), Color, Duration);
    Scene.GetDebugDrawQueue().AddLine(
        Position - FVector(0, Size, 0), Position + FVector(0, Size, 0), Color, Duration);
    Scene.GetDebugDrawQueue().AddLine(
        Position - FVector(0, 0, Size), Position + FVector(0, 0, Size), Color, Duration);
}

// ======================================================================
// 기존 UWorld* 버전 (FScene& 버전을 래핑하여 재사용)
// ======================================================================

void DrawDebugLine(UWorld* World,
                   const FVector& Start, const FVector& End,
                   const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugLine(World->GetScene(), Start, End, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& Center, const FVector& Extent,
                  const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugBox(World->GetScene(), Center, Extent, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugBox(World->GetScene(), P0, P1, P2, P3, Color, Duration);
}

void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FVector& P4, const FVector& P5,
                  const FVector& P6, const FVector& P7,
                  const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugBox(World->GetScene(), P0, P1, P2, P3, P4, P5, P6, P7, Color, Duration);
}

void DrawDebugSphere(UWorld* World,
                     const FVector& Center, float Radius,
                     int32 Segments, const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugSphere(World->GetScene(), Center, Radius, Segments, Color, Duration);
}

void DrawDebugPoint(UWorld* World,
                    const FVector& Position, float Size,
                    const FColor& Color, float Duration)
{
    if (!World) return;
    DrawDebugPoint(World->GetScene(), Position, Size, Color, Duration);
}

#endif // _DEBUG