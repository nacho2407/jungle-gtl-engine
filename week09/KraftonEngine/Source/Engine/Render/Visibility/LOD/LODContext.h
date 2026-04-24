// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Core/CoreTypes.h"

// ── LOD 거리 임계값 (제곱) ──
static constexpr float  LOD1_DIST_SQ                        = 15.0f * 15.0f;
static constexpr float  LOD2_DIST_SQ                        = 25.0f * 25.0f;
static constexpr float  LOD3_DIST_SQ                        = 40.0f * 40.0f;
static constexpr float  LOD1_BACK_SQ                        = 13.5f * 13.5f;
static constexpr float  LOD2_BACK_SQ                        = 22.5f * 22.5f;
static constexpr float  LOD3_BACK_SQ                        = 36.0f * 36.0f;
static constexpr uint32 LOD_UPDATE_SLICE_COUNT              = 4;
static constexpr uint32 LOD_STAGGER_MIN_VISIBLE             = 2048;
static constexpr float  LOD_FULL_UPDATE_CAMERA_MOVE_SQ      = 1.0f * 1.0f;
static constexpr float  LOD_FULL_UPDATE_CAMERA_ROTATION_DOT = 0.9986295f;

static_assert((LOD_UPDATE_SLICE_COUNT & (LOD_UPDATE_SLICE_COUNT - 1)) == 0,
              "LOD_UPDATE_SLICE_COUNT must be a power of two.");

inline uint32 SelectLOD(uint32 CurLOD, float DistSq)
{
    uint32 LOD = CurLOD;
    if (CurLOD == 0 && DistSq > LOD1_DIST_SQ)
        LOD = 1;
    if (CurLOD <= 1 && DistSq > LOD2_DIST_SQ)
        LOD = 2;
    if (CurLOD <= 2 && DistSq > LOD3_DIST_SQ)
        LOD = 3;
    if (CurLOD == 3 && DistSq < LOD3_BACK_SQ)
        LOD = 2;
    if (CurLOD >= 2 && DistSq < LOD2_BACK_SQ)
        LOD = 1;
    if (CurLOD >= 1 && DistSq < LOD1_BACK_SQ)
        LOD = 0;
    return LOD;
}

// FLODUpdateContext는 실행 중 공유되는 상태와 참조를 묶어 전달합니다.
struct FLODUpdateContext
{
    FVector CameraPos;
    uint32  LODUpdateFrame    = 0;
    uint32  LODUpdateSlice    = 0;
    bool    bForceFullRefresh = true;
    bool    bValid            = false;

    inline bool ShouldRefreshLOD(uint32 ProxyId, uint32 LastLODUpdateFrame) const
    {
        return bForceFullRefresh || LastLODUpdateFrame == UINT32_MAX || ((ProxyId & (LOD_UPDATE_SLICE_COUNT - 1)) == LODUpdateSlice);
    }
};
