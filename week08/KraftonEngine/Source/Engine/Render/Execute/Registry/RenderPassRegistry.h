#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassPresets.h"
#include "Render/Execute/Passes/Base/RenderPass.h"

class FDepthPrePass;
class FOpaquePass;
class FDecalPass;
class FLightingPass;
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

enum class ERenderPassNodeType
{
    GridPass,
    DepthPrePass,
    LightCullingPass,
    OpaquePass,
    DecalPass,
    LightingPass,
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

// Owns render pass instances and the pass execution presets used by draw submission.
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
