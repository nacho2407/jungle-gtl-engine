// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

namespace ERenderCBKey
{
constexpr uint32 Gizmo      = 0;
constexpr uint32 Fog        = 2;
constexpr uint32 Outline    = 3;
constexpr uint32 SceneDepth = 4;
constexpr uint32 FXAA       = 5;
constexpr uint32 Light      = 6;
} // namespace ERenderCBKey
