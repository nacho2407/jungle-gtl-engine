// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassRegistry.h"

#include "Render/Execute/Passes/Editor/DebugLinePass.h"
#include "Render/Execute/Passes/Editor/GizmoPass.h"
#include "Render/Execute/Passes/Editor/OverlayBillboardPass.h"
#include "Render/Execute/Passes/Editor/OutlinePass.h"
#include "Render/Execute/Passes/Editor/OverlayTextPass.h"
#include "Render/Execute/Passes/Editor/SelectionMaskPass.h"
#include "Render/Execute/Passes/Editor/LightHitMapPass.h"
#include "Render/Execute/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Execute/Passes/Scene/AlphaBlendPass.h"
#include "Render/Execute/Passes/Scene/OpaquePass.h"
#include "Render/Execute/Passes/Scene/DecalPass.h"
#include "Render/Execute/Passes/Scene/DepthPrePass.h"
#include "Render/Execute/Passes/Scene/FXAAPass.h"
#include "Render/Execute/Passes/Scene/HeightFogPass.h"
#include "Render/Execute/Passes/Scene/LightCullingPass.h"
#include "Render/Execute/Passes/Scene/LightingPass.h"
#include "Render/Execute/Passes/Scene/PresentPass.h"
#include "Render/Execute/Passes/Scene/NonLitViewModePass.h"

// ========== Lifecycle ==========

FRenderPassRegistry::~FRenderPassRegistry()
{
    Release();
}

void FRenderPassRegistry::Initialize()
{
    Release();

    InitializeDefaultRenderPassPresets(RenderPassPresets);

    // ---------- Scene Passes ----------
    Passes.emplace((int32)ERenderPassNodeType::DepthPrePass, new FDepthPrePass());
    Passes.emplace((int32)ERenderPassNodeType::LightCullingPass, new FLightCullingPass());
    Passes.emplace((int32)ERenderPassNodeType::OpaquePass, new FOpaquePass());
    Passes.emplace((int32)ERenderPassNodeType::DecalPass, new FDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::LightingPass, new FLightingPass());
    Passes.emplace((int32)ERenderPassNodeType::AdditiveDecalPass, new FAdditiveDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::AlphaBlendPass, new FAlphaBlendPass());
    Passes.emplace((int32)ERenderPassNodeType::NonLitViewModePass, new FNonLitViewModePass());
    Passes.emplace((int32)ERenderPassNodeType::HeightFogPass, new FHeightFogPass());
    Passes.emplace((int32)ERenderPassNodeType::FXAAPass, new FFXAAPass());
    Passes.emplace((int32)ERenderPassNodeType::PresentPass, new FPresentPass());

    // ---------- Editor And Overlay Passes ----------
    Passes.emplace((int32)ERenderPassNodeType::SelectionMaskPass, new FSelectionMaskPass());
    Passes.emplace((int32)ERenderPassNodeType::OutlinePass, new FOutlinePass());
    Passes.emplace((int32)ERenderPassNodeType::DebugLinePass, new FDebugLinePass());
    Passes.emplace((int32)ERenderPassNodeType::OverlayBillboardPass, new FOverlayBillboardPass());
    Passes.emplace((int32)ERenderPassNodeType::GizmoPass, new FGizmoPass());
    Passes.emplace((int32)ERenderPassNodeType::OverlayTextPass, new FOverlayTextPass());
    Passes.emplace((int32)ERenderPassNodeType::LightHitMapPass, new FLightHitMapPass());
}

void FRenderPassRegistry::Release()
{
    for (auto& Pair : Passes)
    {
        delete Pair.second;
    }

    Passes.clear();
}

// ========== Lookup ==========

FRenderPass* FRenderPassRegistry::FindPass(ERenderPassNodeType Type) const
{
    auto It = Passes.find((int32)Type);
    return It != Passes.end() ? It->second : nullptr;
}

const FRenderPassPreset& FRenderPassRegistry::GetRenderPassPreset(ERenderPass Pass) const
{
    return RenderPassPresets[(uint32)Pass];
}

const FRenderPassDrawPreset& FRenderPassRegistry::GetRenderPassDrawPreset(ERenderPass Pass) const
{
    return RenderPassPresets[(uint32)Pass].Draw;
}

const FRenderPassPreset* FRenderPassRegistry::GetRenderPassPresets() const
{
    return RenderPassPresets;
}
