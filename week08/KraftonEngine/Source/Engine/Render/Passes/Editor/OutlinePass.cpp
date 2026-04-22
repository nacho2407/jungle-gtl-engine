#include "Render/Passes/Editor/OutlinePass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

void FOutlinePass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.SceneView)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    CopyViewportDepthToReadable(Context);
    BindSceneColorInput(Context);
    BindDepthInput(Context);
    BindStencilInput(Context);
}


void FOutlinePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::Outline);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FOutlinePostProcessConstants Constants = {};
    Constants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    Constants.OutlineThickness = 1.0f;

    FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ERenderCBKey::Outline, sizeof(FOutlinePostProcessConstants));
    if (!OutlineCB)
    {
        return;
    }

    OutlineCB->Update(Context.Context, &Constants, sizeof(Constants));
    Context.DrawCommandList->GetCommands().back().PerShaderCB[0] = OutlineCB;
}

void FOutlinePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            if ((c.SortKey & 0xFFFu) == ToPostProcessUserBits(EViewModePostProcessVariant::Outline))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
