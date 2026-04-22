#pragma once

#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

class FShader;
class FViewModePassRegistry;
struct FRenderPipelinePassDesc;

inline FShader* ResolvePipelineShader(const FViewModePassRegistry* ViewModePassRegistry, EViewMode ViewMode, ERenderPass Pass, FShader* FallbackShader)
{
    if (!ViewModePassRegistry)
    {
        return FallbackShader;
    }

    EViewModeStage Stage = EViewModeStage::Opaque;
    switch (Pass)
    {
    case ERenderPass::Opaque:
        Stage = EViewModeStage::Opaque;
        break;
    case ERenderPass::Decal:
        Stage = EViewModeStage::Decal;
        break;
    case ERenderPass::Lighting:
        Stage = EViewModeStage::Lighting;
        break;
    default:
        return FallbackShader;
    }

    if (const FRenderPipelinePassDesc* PassDesc = ViewModePassRegistry->FindPassDesc(ViewMode, Stage))
    {
        if (PassDesc->CompiledShader)
        {
            return PassDesc->CompiledShader;
        }
    }

    return FallbackShader;
}
