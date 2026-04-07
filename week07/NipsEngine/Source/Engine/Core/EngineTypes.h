#pragma once

#include "Core/CoreTypes.h"
#include "Math/Color.h"
#include "Engine/Geometry/AABB.h"

//	TODO : 나중에 지우기
using FBoundingBox = FAABB;

// ============================================================
// EViewModeIndex — 렌더링 뷰 모드
// ============================================================
enum class EViewModeIndex
{
	Lit,        // 기본 라이팅 적용
	Unlit,      // 라이팅 없음
	Wireframe   // 와이어프레임
};

// ============================================================
// EEngineShowFlags — 렌더링 요소별 On/Off 비트 플래그
// ============================================================
enum EEngineShowFlags : uint32
{
	SF_None = 0,
	SF_Primitives = 1 << 0,   // 프리미티브 메시
	SF_Grid = 1 << 1,   // 월드 그리드
	SF_BoundingBox = 1 << 2,   // AABB 바운딩 박스
	SF_BillboardText = 1 << 3,   // 빌보드 UUID 텍스트
	SF_Gizmo = 1 << 4,   // 트랜스폼 기즈모

	SF_All = SF_Primitives | SF_Grid | SF_BoundingBox | SF_BillboardText | SF_Gizmo
};
