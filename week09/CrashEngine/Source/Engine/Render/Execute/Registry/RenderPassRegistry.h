// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassPresets.h"
#include "Render/Execute/Passes/Base/RenderPass.h"

class FDepthPrePass;
class FDeferredOpaquePass;
class FForwardOpaquePass;
class FDeferredDecalPass;
class FForwardDecalPass;
class FDeferredLightingPass;
class FAdditiveDecalPass;
class FAlphaBlendPass;
class FHeightFogPass;
class FNonLitViewModePass;
class FFXAAPass;
class FPresentPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinePass;
class FGizmoPass;
class FOverlayBillboardPass;
class FOverlayTextPass;

// ERenderPassNodeType는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ERenderPassNodeType
{
    GridPass,
    DepthPrePass,
    ShadowMapPass,
    LightCullingPass,
    DeferredOpaquePass,
    ForwardOpaquePass,
    DeferredDecalPass,
    ForwardDecalPass,
    DeferredLightingPass,
    AdditiveDecalPass,
    AlphaBlendPass,
    NonLitViewModePass,
    HeightFogPass,
    FXAAPass,
    PresentPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinePass,
    OverlayBillboardPass,
    GizmoPass,
    OverlayTextPass,
    LightHitMapPass,
};

// FRenderPassRegistry는 실행 시 필요한 타입과 규칙의 매핑을 보관합니다.
class FRenderPassRegistry
{
public:
    FRenderPassRegistry() = default;
    ~FRenderPassRegistry();

    void Initialize();
    void Release();

    FRenderPass*                 FindPass(ERenderPassNodeType Type) const;
    const FRenderPassPreset&     GetRenderPassPreset(ERenderPass Pass) const;
    const FRenderPassDrawPreset& GetRenderPassDrawPreset(ERenderPass Pass) const;
    const FRenderPassPreset*     GetRenderPassPresets() const;

private:
    TMap<int32, FRenderPass*> Passes;
    FRenderPassPreset         RenderPassPresets[(uint32)ERenderPass::MAX] = {};
};
