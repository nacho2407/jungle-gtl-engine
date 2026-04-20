#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/RenderStateTypes.h"

// Mesh Shape Enum — MeshBufferManager 조회용 (순수 기하 형상)
enum class EMeshShape
{
    Cube,
    Sphere,
    Plane,
    Quad,
    TexturedQuad,
    TransGizmo,
    RotGizmo,
    ScaleGizmo,
};

enum class ELightType
{
    Directional,
    Point,
    Spot,
    Ambient,
};

enum class ERenderPass : uint32
{
    DepthPre,
    Opaque,
    Decal,
    Lighting,
    AdditiveDecal,
    AlphaBlend,
    SelectionMask,
    EditorLines,
    PostProcess,
    FXAA,
    GizmoOuter,
    GizmoInner,
    OverlayFont,
    MAX
};

inline const char* GetRenderPassName(ERenderPass Pass)
{
    static const char* Names[] = {
        "RenderPass::DepthPre",
        "RenderPass::Opaque",
        "RenderPass::Decal",
        "RenderPass::Lighting",
        "RenderPass::AdditiveDecal",
        "RenderPass::AlphaBlend",
        "RenderPass::SelectionMask",
        "RenderPass::EditorLines",
        "RenderPass::PostProcess",
        "RenderPass::FXAA",
        "RenderPass::GizmoOuter",
        "RenderPass::GizmoInner",
        "RenderPass::OverlayFont",
    };
    static_assert(ARRAYSIZE(Names) == (uint32)ERenderPass::MAX, "Names must match ERenderPass entries");
    return Names[(uint32)Pass];
}

namespace RenderStateStrings
{
    inline constexpr FEnumEntry RenderPassMap[] =
    {
        { "DepthPre",      (int)ERenderPass::DepthPre },
        { "Opaque",        (int)ERenderPass::Opaque },
        { "Decal",         (int)ERenderPass::Decal },
        { "Lighting",      (int)ERenderPass::Lighting },
        { "AdditiveDecal", (int)ERenderPass::AdditiveDecal },
        { "AlphaBlend",    (int)ERenderPass::AlphaBlend },
        { "SelectionMask", (int)ERenderPass::SelectionMask },
        { "EditorLines",   (int)ERenderPass::EditorLines },
        { "PostProcess",   (int)ERenderPass::PostProcess },
        { "FXAA",          (int)ERenderPass::FXAA },
        { "GizmoOuter",    (int)ERenderPass::GizmoOuter },
        { "GizmoInner",    (int)ERenderPass::GizmoInner },
        { "OverlayFont",   (int)ERenderPass::OverlayFont },
    };

    static_assert(ARRAYSIZE(RenderPassMap) == (int)ERenderPass::MAX, "RenderPassMap must match ERenderPass entries");
}
