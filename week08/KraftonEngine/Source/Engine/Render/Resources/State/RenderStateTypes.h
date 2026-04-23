#pragma once

#include "Core/CoreTypes.h"

// Logical render state identifiers used by pass state descriptions.
enum class EDepthStencilState
{
    Default,
    DepthReadOnly,
    StencilWrite,
    StencilWriteOnlyEqual,
    NoDepth,
    GizmoInside,
    GizmoOutside,
    MAX
};

enum class EBlendState
{
    Opaque,
    AlphaBlend,
    Additive,
    NoColor,
    MAX
};

enum class ERasterizerState
{
    SolidBackCull,
    SolidFrontCull,
    SolidNoCull,
    WireFrame,
    MAX
};
