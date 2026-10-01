// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Debug/DebugRenderAPI.h"

#if defined(_DRAW_DEBUG_ENABLE)

#include "GameFramework/World.h"
#include "Render/Scene/Scene.h"

void RenderDebugLine(FScene&        Scene,
                     const FVector& Start, const FVector& End,
                     const FColor& Color, float Duration)
{
    Scene.GetDebugPrimitiveQueue().AddLine(Start, End, Color, Duration);
}

void RenderDebugBox(FScene&        Scene,
                    const FVector& Center, const FVector& Extent,
                    const FColor& Color, float Duration)
{
    Scene.GetDebugPrimitiveQueue().AddBox(Center, Extent, Color, Duration);
}

void RenderDebugBox(FScene&        Scene,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FColor& Color, float Duration)
{
    Scene.GetDebugPrimitiveQueue().AddBox(P0, P1, P2, P3, Color, Duration);
}

void RenderDebugBox(FScene&        Scene,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FVector& P4, const FVector& P5,
                    const FVector& P6, const FVector& P7,
                    const FColor& Color, float Duration)
{
    Scene.GetDebugPrimitiveQueue().AddBox(P0, P1, P2, P3, P4, P5, P6, P7, Color, Duration);
}

void RenderDebugSphere(FScene&        Scene,
                       const FVector& Center, float Radius,
                       int32 Segments, const FColor& Color, float Duration)
{
    Scene.GetDebugPrimitiveQueue().AddSphere(Center, Radius, Segments, Color, Duration);
}

void RenderDebugArrow(FScene&        Scene,
                      const FVector& Start, const FVector& Direction,
                      float         Length,
                      const FColor& Color, float Duration, int32 Segments)
{
    Scene.GetDebugPrimitiveQueue().AddArrow(Start, Direction, Length, Color, Duration, Segments);
}

void RenderDebugPoint(FScene&        Scene,
                      const FVector& Position, float Size,
                      const FColor& Color, float Duration)
{
    RenderDebugLine(Scene, Position - FVector(Size, 0.0f, 0.0f), Position + FVector(Size, 0.0f, 0.0f), Color, Duration);
    RenderDebugLine(Scene, Position - FVector(0.0f, Size, 0.0f), Position + FVector(0.0f, Size, 0.0f), Color, Duration);
    RenderDebugLine(Scene, Position - FVector(0.0f, 0.0f, Size), Position + FVector(0.0f, 0.0f, Size), Color, Duration);
}

void RenderDebugLine(UWorld*        World,
                     const FVector& Start, const FVector& End,
                     const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugLine(World->GetScene(), Start, End, Color, Duration);
    }
}

void RenderDebugBox(UWorld*        World,
                    const FVector& Center, const FVector& Extent,
                    const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugBox(World->GetScene(), Center, Extent, Color, Duration);
    }
}

void RenderDebugBox(UWorld*        World,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugBox(World->GetScene(), P0, P1, P2, P3, Color, Duration);
    }
}

void RenderDebugBox(UWorld*        World,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FVector& P4, const FVector& P5,
                    const FVector& P6, const FVector& P7,
                    const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugBox(World->GetScene(), P0, P1, P2, P3, P4, P5, P6, P7, Color, Duration);
    }
}

void RenderDebugSphere(UWorld*        World,
                       const FVector& Center, float Radius,
                       int32 Segments, const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugSphere(World->GetScene(), Center, Radius, Segments, Color, Duration);
    }
}

void RenderDebugArrow(UWorld*        World,
                      const FVector& Start, const FVector& Direction,
                      float         Length,
                      const FColor& Color, float Duration, int32 Segments)
{
    if (World)
    {
        RenderDebugArrow(World->GetScene(), Start, Direction, Length, Color, Duration, Segments);
    }
}

void RenderDebugPoint(UWorld*        World,
                      const FVector& Position, float Size,
                      const FColor& Color, float Duration)
{
    if (World)
    {
        RenderDebugPoint(World->GetScene(), Position, Size, Color, Duration);
    }
}

#endif
