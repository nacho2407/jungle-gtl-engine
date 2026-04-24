// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

class FScene;
class UWorld;

#define _DRAW_DEBUG_ENABLE
#if defined(_DRAW_DEBUG_ENABLE)

void RenderDebugLine(FScene&        Scene,
                     const FVector& Start, const FVector& End,
                     const FColor& Color    = FColor::White(),
                     float         Duration = 0.0f);

void RenderDebugBox(FScene&        Scene,
                    const FVector& Center, const FVector& Extent,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugBox(FScene&        Scene,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugBox(FScene&        Scene,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FVector& P4, const FVector& P5,
                    const FVector& P6, const FVector& P7,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugSphere(FScene&        Scene,
                       const FVector& Center, float Radius,
                       int32         Segments = 16,
                       const FColor& Color    = FColor::White(),
                       float         Duration = 0.0f);

void RenderDebugArrow(FScene&        Scene,
                      const FVector& Start, const FVector& Direction,
                      float         Length,
                      const FColor& Color    = FColor::White(),
                      float         Duration = 0.0f,
                      int32         Segments = 8);

void RenderDebugPoint(FScene&        Scene,
                      const FVector& Position, float Size = 0.1f,
                      const FColor& Color    = FColor::White(),
                      float         Duration = 0.0f);

void RenderDebugLine(UWorld*        World,
                     const FVector& Start, const FVector& End,
                     const FColor& Color    = FColor::White(),
                     float         Duration = 0.0f);

void RenderDebugBox(UWorld*        World,
                    const FVector& Center, const FVector& Extent,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugBox(UWorld*        World,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugBox(UWorld*        World,
                    const FVector& P0, const FVector& P1,
                    const FVector& P2, const FVector& P3,
                    const FVector& P4, const FVector& P5,
                    const FVector& P6, const FVector& P7,
                    const FColor& Color    = FColor::White(),
                    float         Duration = 0.0f);

void RenderDebugSphere(UWorld*        World,
                       const FVector& Center, float Radius,
                       int32         Segments = 16,
                       const FColor& Color    = FColor::White(),
                       float         Duration = 0.0f);

void RenderDebugArrow(UWorld*        World,
                      const FVector& Start, const FVector& Direction,
                      float         Length,
                      const FColor& Color    = FColor::White(),
                      float         Duration = 0.0f,
                      int32         Segments = 8);

void RenderDebugPoint(UWorld*        World,
                      const FVector& Position, float Size = 0.1f,
                      const FColor& Color    = FColor::White(),
                      float         Duration = 0.0f);

#else

inline void RenderDebugLine(FScene&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(FScene&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(FScene&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(FScene&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugSphere(FScene&, const FVector&, float, int32 = 16, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugArrow(FScene&, const FVector&, const FVector&, float, const FColor& = FColor::White(), float = 0.0f, int32 = 8) {}
inline void RenderDebugPoint(FScene&, const FVector&, float = 0.1f, const FColor& = FColor::White(), float = 0.0f) {}

inline void RenderDebugLine(UWorld*, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(UWorld*, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(UWorld*, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugBox(UWorld*, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugSphere(UWorld*, const FVector&, float, int32 = 16, const FColor& = FColor::White(), float = 0.0f) {}
inline void RenderDebugArrow(UWorld*, const FVector&, const FVector&, float, const FColor& = FColor::White(), float = 0.0f, int32 = 8) {}
inline void RenderDebugPoint(UWorld*, const FVector&, float = 0.1f, const FColor& = FColor::White(), float = 0.0f) {}

#endif
