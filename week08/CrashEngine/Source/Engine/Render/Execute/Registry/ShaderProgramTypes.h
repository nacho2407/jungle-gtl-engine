#pragma once

#include "Core/CoreTypes.h"

// Stable keys for built-in VS/PS shader programs used by render passes and scene proxies.
enum class EShaderType : uint32
{
    Default = 0,
    Primitive,
    Gizmo,
    Editor,
    StaticMesh,
    Decal,
    OutlinePostProcess,
    Font,
    OverlayFont,
    SubUV,
    Billboard,
    HeightFog,
    DepthOnly,
    ShadowEncodeVSM,
    ShadowEncodeESM,
    SceneDepth,
    NormalView,
    FXAA,
    LightHitMap,
    MAX,
};
