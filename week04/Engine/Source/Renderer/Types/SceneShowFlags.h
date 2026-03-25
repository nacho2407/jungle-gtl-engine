#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Misc/BitMaskEnum.h"

enum class ESceneShowFlags : uint64
{
    None = 0,
    SF_Primitives = 1ull << 0,
    SF_BillboardText = 1ull << 1,
    SF_Sprites = 1ull << 2,
    SF_UUID = 1ull << 3
};

template <> struct TEnableBitMaskOperators<ESceneShowFlags>
{
    static constexpr bool bEnabled = true;
};