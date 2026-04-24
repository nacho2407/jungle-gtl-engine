// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

/*
    드로우 커맨드가 어떤 렌더 패스에 속하는지 나타내는 enum입니다.
    제출 정렬, 패스 범위 계산, 파이프라인 실행 순서에서 공통으로 사용합니다.
*/
enum class ERenderPass : uint32
{
    Grid,
    DepthPre,
    Opaque,
    Decal,
    DeferredLighting,
    AdditiveDecal,
    AlphaBlend,
    SelectionMask,
    EditorLines,
    PostProcess,
    FXAA,
    GizmoOuter,
    GizmoInner,
    OverlayBillboard,
    OverlayTextWorld,
    OverlayFont,
    MAX
};

inline const char* GetRenderPassName(ERenderPass Pass)
{
    static const char* Names[] = {
        "RenderPass::Grid",
        "RenderPass::DepthPre",
        "RenderPass::Opaque",
        "RenderPass::Decal",
        "RenderPass::DeferredLighting",
        "RenderPass::AdditiveDecal",
        "RenderPass::AlphaBlend",
        "RenderPass::SelectionMask",
        "RenderPass::EditorLines",
        "RenderPass::PostProcess",
        "RenderPass::FXAA",
        "RenderPass::GizmoOuter",
        "RenderPass::GizmoInner",
        "RenderPass::OverlayBillboard",
        "RenderPass::OverlayTextWorld",
        "RenderPass::OverlayFont",
    };
    static_assert(ARRAY_SIZE(Names) == (uint32)ERenderPass::MAX, "Names must match ERenderPass entries");
    return Names[(uint32)Pass];
}
