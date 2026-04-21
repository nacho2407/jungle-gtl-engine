#include "Render/Pipelines/Registry/RenderPassRegistry.h"

#include "Render/Passes/Editor/DebugLinePass.h"
#include "Render/Passes/Editor/GizmoPass.h"
#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Passes/Editor/OverlayTextPass.h"
#include "Render/Passes/Editor/SelectionMaskPass.h"
#include "Render/Passes/Scene/AdditiveDecalPass.h"
#include "Render/Passes/Scene/AlphaBlendPass.h"
#include "Render/Passes/Scene/BaseDrawPass.h"
#include "Render/Passes/Scene/DecalPass.h"
#include "Render/Passes/Scene/DepthPrePass.h"
#include "Render/Passes/Scene/FXAAPass.h"
#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Passes/Scene/LightCullingPass.h"
#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Passes/Scene/ViewModeResolvePass.h"

FRenderPassRegistry::~FRenderPassRegistry()
{
    Release();
}

void FRenderPassRegistry::Initialize()
{
    Release();

    InitializeDefaultPassRenderStateDescs(PassStateDescs);

    Passes.emplace((int32)ERenderPassNodeType::DepthPrePass, new FDepthPrePass());
    Passes.emplace((int32)ERenderPassNodeType::LightCullingPass, new FLightCullingPass());
    Passes.emplace((int32)ERenderPassNodeType::BaseDrawPass, new FBaseDrawPass());
    Passes.emplace((int32)ERenderPassNodeType::DecalPass, new FDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::LightingPass, new FLightingPass());
    Passes.emplace((int32)ERenderPassNodeType::AdditiveDecalPass, new FAdditiveDecalPass());
    Passes.emplace((int32)ERenderPassNodeType::AlphaBlendPass, new FAlphaBlendPass());
    Passes.emplace((int32)ERenderPassNodeType::ViewModeResolvePass, new FViewModeResolvePass());
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

const FPassRenderStateDesc& FRenderPassRegistry::GetPassStateDesc(ERenderPass Pass) const
{
    return PassStateDescs[(uint32)Pass];
}

const FPassRenderStateDesc* FRenderPassRegistry::GetPassStateDescs() const
{
    return PassStateDescs;
}
