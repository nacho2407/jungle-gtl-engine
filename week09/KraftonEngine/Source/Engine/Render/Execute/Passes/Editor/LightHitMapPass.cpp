// 렌더 영역의 세부 동작을 구현합니다.
#include "LightHitMapPass.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"

void FLightHitMapPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.SceneView)
    {
        return;
    }
    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);

    if (Context.LightCulling)
    {
        ID3D11ShaderResourceView* HipMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(8, 1, &HipMapSRV);
    }
}

void FLightHitMapPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.SceneView || !Context.SceneView->ShowFlags.bLightHitMap)
    {
        return;
    }

    const FViewportRenderTargets* Targets = Context.Targets;

    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::LightHitMap);
}

void FLightHitMapPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            if ((c.SortKey & 0xFFFu) == ToPostProcessUserBits(EViewModePostProcessVariant::LightHitMap))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }

    ID3D11ShaderResourceView* NullSRV = { nullptr };
    Context.Context->PSSetShaderResources(8, 1, &NullSRV);
}
