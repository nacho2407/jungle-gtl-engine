#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

class UWorld;
class FScene; // FScene 전방 선언 추가

// ============================================================
// UE 스타일 글로벌 디버그 드로우 함수
//
// Duration == 0: 1프레임만 표시
// Duration > 0 : 지정 시간(초) 동안 표시
//
// _DEBUG 빌드에서만 동작, Release/Demo에서는 no-op
// ============================================================

#define _DRAW_DEBUG_ENABLE
#if defined(_DRAW_DEBUG_ENABLE)

// ------------------------------------------------------------
// FScene& 오버로딩 버전
// ------------------------------------------------------------

void DrawDebugLine(FScene& Scene,
                   const FVector& Start, const FVector& End,
                   const FColor& Color = FColor::White(),
                   float Duration = 0.0f);

void DrawDebugBox(FScene& Scene,
                  const FVector& Center, const FVector& Extent,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

void DrawDebugBox(FScene& Scene,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

void DrawDebugBox(FScene& Scene,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FVector& P4, const FVector& P5,
                  const FVector& P6, const FVector& P7,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

void DrawDebugSphere(FScene& Scene,
                     const FVector& Center, float Radius,
                     int32 Segments = 16,
                     const FColor& Color = FColor::White(),
                     float Duration = 0.0f);

void DrawDebugPoint(FScene& Scene,
                    const FVector& Position, float Size = 0.1f,
                    const FColor& Color = FColor::White(),
                    float Duration = 0.0f);

// ------------------------------------------------------------
// 기존 UWorld* 래퍼 버전
// ------------------------------------------------------------

void DrawDebugLine(UWorld* World,
                   const FVector& Start, const FVector& End,
                   const FColor& Color = FColor::White(),
                   float Duration = 0.0f);

void DrawDebugBox(UWorld* World,
                  const FVector& Center, const FVector& Extent,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

// 4개 꼭짓점으로 사각형(Quad) 외곽선 그리기 (P0→P1→P2→P3→P0)
void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

// 8개 꼭짓점 직육면체 — 하단면(P0~P3) + 상단면(P4~P7), 12에지
//   하단: P0→P1→P2→P3→P0
//   상단: P4→P5→P6→P7→P4
//   수직: P0→P4, P1→P5, P2→P6, P3→P7
void DrawDebugBox(UWorld* World,
                  const FVector& P0, const FVector& P1,
                  const FVector& P2, const FVector& P3,
                  const FVector& P4, const FVector& P5,
                  const FVector& P6, const FVector& P7,
                  const FColor& Color = FColor::White(),
                  float Duration = 0.0f);

void DrawDebugSphere(UWorld* World,
                     const FVector& Center, float Radius,
                     int32 Segments = 16,
                     const FColor& Color = FColor::White(),
                     float Duration = 0.0f);

void DrawDebugPoint(UWorld* World,
                    const FVector& Position, float Size = 0.1f,
                    const FColor& Color = FColor::White(),
                    float Duration = 0.0f);

#else

// ------------------------------------------------------------
// Release / No-op (FScene& 버전)
// ------------------------------------------------------------
inline void DrawDebugLine(FScene&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(FScene&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(FScene&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(FScene&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugSphere(FScene&, const FVector&, float, int32 = 16, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugPoint(FScene&, const FVector&, float = 0.1f, const FColor& = FColor::White(), float = 0.0f) {}

// ------------------------------------------------------------
// Release / No-op (UWorld* 버전)
// ------------------------------------------------------------
inline void DrawDebugLine(UWorld*, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(UWorld*, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(UWorld*, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugBox(UWorld*, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FVector&, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugSphere(UWorld*, const FVector&, float, int32 = 16, const FColor& = FColor::White(), float = 0.0f) {}
inline void DrawDebugPoint(UWorld*, const FVector&, float = 0.1f, const FColor& = FColor::White(), float = 0.0f) {}

#endif