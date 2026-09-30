// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Passes/Editor/OutlinePass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

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
    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::Outline);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FOutlinePostProcessCBData Constants = {};
    Constants.OutlineColor              = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    Constants.OutlineThickness          = 2.5f;

    FConstantBuffer* OutlineCB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::Outline, sizeof(FOutlinePostProcessCBData));
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
            const uint8 UserBits = static_cast<uint8>((c.SortKey >> 52) & 0xFFu);
            if (UserBits == static_cast<uint8>(ToPostProcessUserBits(EViewModePostProcessVariant::Outline)))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}

