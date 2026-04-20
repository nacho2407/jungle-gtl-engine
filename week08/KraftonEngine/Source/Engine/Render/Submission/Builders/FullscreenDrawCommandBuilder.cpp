#include "Render/Submission/Builders/FullscreenDrawCommandBuilder.h"
#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Commands/DrawCommand.h"
#include "Render/Resources/Managers/ShaderManager.h"
#include "Render/Passes/Common/PassRenderState.h"
#include "Render/Pipelines/ViewMode/ViewModePassConfig.h"
#include "Render/View/SceneView.h"
#include "Render/View/ViewModeSurfaceSet.h"
#include "Render/View/ViewportRenderTargets.h"

void FFullscreenDrawCommandBuilder::Build(ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList, uint16 UserBits)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    FShader* Shader = nullptr;

    if (Pass == ERenderPass::Lighting)
    {
        if (!Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
        {
            return;
        }

        const FRenderPipelinePassDesc* Desc = Context.ViewModePassRegistry->FindPassDesc(Context.ActiveViewMode, EPipelineStage::Lighting);
        if (!Desc || !Desc->CompiledShader)
        {
            return;
        }

        Shader = Desc->CompiledShader;
    }
    else if (Pass == ERenderPass::FXAA)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::FXAA);
    }
    else if (Pass == ERenderPass::PostProcess)
    {
        switch (UserBits)
        {
        case 1:
            Shader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
            break;
        case 2:
            Shader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
            break;
        case 3:
            Shader = FShaderManager::Get().GetShader(EShaderType::NormalView);
            break;
        default:
            Shader = FShaderManager::Get().GetShader(EShaderType::HeightFog);
            break;
        }
    }

    if (!Shader)
        return;

    const FPassRenderState& S = Context.GetPassState(Pass);
    FDrawCommand& Cmd = OutList.AddCommand();
    Cmd.Shader = Shader;
    Cmd.DepthStencil = S.DepthStencil;
    Cmd.Blend = S.Blend;
    Cmd.Rasterizer = S.Rasterizer;
    Cmd.Topology = S.Topology;
    Cmd.VertexCount = 3;
    Cmd.Pass = Pass;

    if (Pass == ERenderPass::Lighting && Context.ActiveViewSurfaceSet)
    {
        // Lighting fullscreen shaders read the base color buffer from t0.
        Cmd.DiffuseSRV = Context.ActiveViewSurfaceSet->GetSRV(ESurfaceSlot::BaseColor);
    }
    else if (Pass == ERenderPass::FXAA && Context.Frame)
    {
        // FXAA prepares SceneColor on t0 before submission; keep the command in sync
        // so SubmitCommand does not overwrite it with nullptr on a forced bind.
        Cmd.DiffuseSRV = Targets ? Targets->SceneColorCopySRV : nullptr;
    }

    Cmd.SortKey = FDrawCommand::BuildSortKey(Pass, Shader, nullptr, Cmd.DiffuseSRV, UserBits);
}
