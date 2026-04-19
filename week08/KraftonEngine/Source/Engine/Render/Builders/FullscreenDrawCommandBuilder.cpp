#include "Render/Builders//FullscreenDrawCommandBuilder.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Core/PassTypes.h"

void FFullscreenDrawCommandBuilder::Build(ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList, uint16 UserBits)
{
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
    Cmd.SortKey = FDrawCommand::BuildSortKey(Pass, Shader, nullptr, nullptr, UserBits);
}
