#include "Render/Pipelines/Registry/RenderPassRegistry.h"

#include "Render/Passes/Geometry/AdditiveDecalPass.h"
#include "Render/Passes/Geometry/AlphaBlendPass.h"
#include "Render/Passes/Geometry/BaseDrawPass.h"
#include "Render/Passes/Editor/DebugLinePass.h"
#include "Render/Passes/Geometry/DecalPass.h"
#include "Render/Passes/Geometry/DepthPrePass.h"
#include "Render/Passes/PostProcess/FXAAPass.h"
#include "Render/Passes/Editor/GizmoPass.h"
#include "Render/Passes/PostProcess/HeightFogPass.h"
#include "Render/Passes/Resolve/LightingPass.h"
#include "Render/Passes/Resolve/ViewModePostProcessPass.h"
#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Passes/Editor/OverlayTextPass.h"
#include "Render/Passes/Editor/SelectionMaskPass.h"

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
    Passes.emplace((int32)ERenderPassNodeType::ViewModePostProcessPass, new FViewModePostProcessPass());
    Passes.emplace((int32)ERenderPassNodeType::HeightFogPass, new FHeightFogPass());
    Passes.emplace((int32)ERenderPassNodeType::FXAAPass, new FFXAAPass());
    Passes.emplace((int32)ERenderPassNodeType::SelectionMaskPass, new FSelectionMaskPass());
    Passes.emplace((int32)ERenderPassNodeType::OutlinePass, new FOutlinePass());
    Passes.emplace((int32)ERenderPassNodeType::DebugLinePass, new FDebugLinePass());
    Passes.emplace((int32)ERenderPassNodeType::GizmoPass, new FGizmoPass());
    Passes.emplace((int32)ERenderPassNodeType::OverlayTextPass, new FOverlayTextPass());
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
