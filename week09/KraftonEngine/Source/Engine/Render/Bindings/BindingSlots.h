#pragma once

#include "Core/CoreTypes.h"

namespace ECBSlot
{
    constexpr uint32 Frame = 0;
    constexpr uint32 PerObject = 1;
    constexpr uint32 PerShader0 = 2;
    constexpr uint32 PerShader1 = 3;
    constexpr uint32 Light = 4;
}

namespace ESystemTexSlot
{
    constexpr uint32 LocalLights = 6;
    constexpr uint32 SceneDepth = 10;
    constexpr uint32 SceneColor = 11;
    constexpr uint32 Stencil = 13;
}

namespace ESamplerSlot
{
    constexpr uint32 LinearClamp = 0;
    constexpr uint32 LinearWrap = 1;
    constexpr uint32 PointClamp = 2;
}

namespace ECBPoolKey
{
    constexpr uint32 Gizmo = 0;
    constexpr uint32 Fog = 2;
    constexpr uint32 Outline = 3;
    constexpr uint32 SceneDepth = 4;
    constexpr uint32 FXAA = 5;
    constexpr uint32 Light = 6;
}
