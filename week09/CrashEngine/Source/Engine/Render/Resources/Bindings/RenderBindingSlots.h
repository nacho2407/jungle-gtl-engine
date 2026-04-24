// 렌더 셰이더 바인딩 슬롯 상수입니다.
// b#: Constant Buffer Register
// t#: Shader Resource Register
// s#: Sampler Register
#pragma once

#include "Core/CoreTypes.h"

// Constant Buffer register slots (b#)
namespace ECBSlot
{
constexpr uint32 Frame      = 0; // b0: frame/view 공용 상수
constexpr uint32 PerObject  = 1; // b1: object 단위 상수
constexpr uint32 PerShader0 = 2; // b2: pass/material 추가 상수
constexpr uint32 PerShader1 = 3; // b3: pass/material 추가 상수
constexpr uint32 Light      = 4; // b4: 전역 조명 상수
} // namespace ECBSlot

// Shader Resource register slots (t#)
namespace ESystemTexSlot
{
constexpr uint32 LocalLights = 6;  // t6: local light buffer
constexpr uint32 SceneDepth  = 10; // t10: scene depth copy
constexpr uint32 SceneColor  = 11; // t11: scene color copy
constexpr uint32 Stencil     = 13; // t13: stencil copy
} // namespace ESystemTexSlot

// Sampler register slots (s#)
namespace ESamplerSlot
{
constexpr uint32 LinearClamp = 0; // s0: linear clamp
constexpr uint32 LinearWrap  = 1; // s1: linear wrap
constexpr uint32 PointClamp  = 2; // s2: point clamp
} // namespace ESamplerSlot