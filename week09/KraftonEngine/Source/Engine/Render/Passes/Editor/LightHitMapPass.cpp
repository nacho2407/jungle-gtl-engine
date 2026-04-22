#include "LightHitMapPass.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Visibility/TileBasedLightCulling.h"

void FLightHitMapPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.SceneView)
    {
        return;
    }
    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);

    // LightCulling 관련 데이터 바이딩
    if (Context.LightCulling)
    {
        // 디버그 용 히트맵 SRV 추가
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

    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::LightHitMap);

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
