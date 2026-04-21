#include "Render/Passes/Scene/HeightFogPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Resources/Buffers/ConstantBufferPool.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Scene/Scene.h"

void FHeightFogPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.SceneView)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    CopyViewportDepthToReadable(Context);
    BindSceneColorInput(Context);
    BindDepthInput(Context);
}


void FHeightFogPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.SceneView || !Context.SceneView->ShowFlags.bFog)
        return;

    if (!Context.Scene || !Context.Scene->HasFog())
        return;

    DrawCommandBuilder::BuildFullscreenDrawCommand(
        ERenderPass::PostProcess,
        Context,
        *Context.DrawCommandList,
        EViewModePostProcessVariant::None);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
        return;

    const FFogParams& Fog = Context.Scene->GetFogParams();

    FFogConstants Constants = {};
    Constants.InscatteringColor = Fog.InscatteringColor;
    Constants.Density = Fog.Density;
    Constants.HeightFalloff = Fog.HeightFalloff;
    Constants.FogBaseHeight = Fog.FogBaseHeight;
    Constants.StartDistance = Fog.StartDistance;
    Constants.CutoffDistance = Fog.CutoffDistance;
    Constants.MaxOpacity = Fog.MaxOpacity;

    FConstantBuffer* FogCB =
        FConstantBufferPool::Get().GetBuffer(ERenderCBKey::Fog, sizeof(FFogConstants));
    if (!FogCB)
        return;

    FogCB->Update(Context.Context, &Constants, sizeof(Constants));
    Context.DrawCommandList->GetCommands().back().PerShaderCB[0] = FogCB;
}

void FHeightFogPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.DrawCommandList)
    {
        uint32 s, e;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, s, e);
        for (uint32 i = s; i < e; ++i)
        {
            const auto& c = Context.DrawCommandList->GetCommands()[i];
            const uint16 UserBits = static_cast<uint16>(c.SortKey & 0xFFFu);
            if (UserBits == ToPostProcessUserBits(EViewModePostProcessVariant::None))
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
