#include "Render/Renderer/RenderPassRegistry.h"

#include "Render/Passes/AdditiveDecalPass.h"
#include "Render/Passes/AlphaBlendPass.h"
#include "Render/Passes/BaseDrawPass.h"
#include "Render/Passes/DebugLinesPass.h"
#include "Render/Passes/DecalPass.h"
#include "Render/Passes/DepthPrePass.h"
#include "Render/Passes/FXAAPass.h"
#include "Render/Passes/GizmoRenderPass.h"
#include "Render/Passes/HeightFogPass.h"
#include "Render/Passes/LightingPass.h"
#include "Render/Passes/OutlinePass.h"
#include "Render/Passes/OverlayFontRenderPass.h"
#include "Render/Passes/SelectionMaskPass.h"

FRenderPassRegistry::~FRenderPassRegistry()
{
    Release();
}

void FRenderPassRegistry::Initialize()
{
    Release();
    Passes.emplace((int32)ERenderPassNodeType::DepthPrePass, new FDepthPrePass());
    Passes.emplace((int32)ERenderPassNodeType::BaseDrawPass, new FBaseDrawPass());
    Passes.emplace((int32)ERenderPassNodeType::DecalPass, new FDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::LightingPass, new FLightingPass());
    Passes.emplace((int32)ERenderPassNodeType::AdditiveDecalPass, new FAdditiveDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::AlphaBlendPass, new FAlphaBlendPass());
    Passes.emplace((int32)ERenderPassNodeType::HeightFogPass, new FHeightFogPass());
    Passes.emplace((int32)ERenderPassNodeType::FXAAPass, new FFXAAPass());
    Passes.emplace((int32)ERenderPassNodeType::SelectionMaskPass, new FSelectionMaskPass());
    Passes.emplace((int32)ERenderPassNodeType::OutlinePass, new FOutlinePass());
    Passes.emplace((int32)ERenderPassNodeType::DebugLinesPass, new FDebugLinesPass());
    Passes.emplace((int32)ERenderPassNodeType::GizmoRenderPass, new FGizmoRenderPass());
    Passes.emplace((int32)ERenderPassNodeType::OverlayFontRenderPass, new FOverlayFontRenderPass());
}

void FRenderPassRegistry::Release()
{
    for (auto& Pair : Passes)
    {
        delete Pair.second;
    }
    Passes.clear();
}

FRenderPass* FRenderPassRegistry::FindPass(ERenderPassNodeType Type) const
{
    auto It = Passes.find((int32)Type);
    return It != Passes.end() ? It->second : nullptr;
}
