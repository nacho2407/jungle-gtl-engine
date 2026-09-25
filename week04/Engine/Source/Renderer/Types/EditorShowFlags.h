#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Misc/BitMaskEnum.h"

enum class EEditorShowFlags : uint64
{
    None = 0,

    SF_Grid = 1ull << 0,
    SF_WorldAxes = 1ull << 1,
    SF_Gizmo = 1ull << 2,
    SF_SelectionOutline = 1ull << 3,
    SF_ObjectLabels = 1ull << 4,
};

template <> struct TEnableBitMaskOperators<EEditorShowFlags>
{
    static constexpr bool bEnabled = true;
};