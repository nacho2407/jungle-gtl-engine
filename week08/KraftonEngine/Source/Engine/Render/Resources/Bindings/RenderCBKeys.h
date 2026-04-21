#pragma once

#include "Core/CoreTypes.h"

namespace ERenderCBKey
{
    // 기즈모 전용 상수 버퍼
    constexpr uint32 Gizmo = 0;

    // Height Fog 전용 상수 버퍼
    constexpr uint32 Fog = 2;

    // Outline 후처리 전용 상수 버퍼
    constexpr uint32 Outline = 3;

    // SceneDepth 시각화 전용 상수 버퍼
    constexpr uint32 SceneDepth = 4;

    // FXAA 전용 상수 버퍼
    constexpr uint32 FXAA = 5;

    // 라이팅/라이트 유틸 전용 상수 버퍼
    constexpr uint32 Light = 6;
} // namespace ERenderCBKey
