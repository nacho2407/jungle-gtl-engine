#include "Render/Passes/Scene/BaseDrawPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Commands/DrawCommandList.h"
#include "Render/Submission/Builders/MeshDrawCommandBuilder.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/View/ViewModeSurfaceSet.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"
#include "Render/Resources/RenderResources.h"

void FBaseDrawPass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(NullSRVs), NullSRVs);

    ID3D11ShaderResourceView* NullSystemSRV = nullptr;
    Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &NullSystemSRV);

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->LocalLightSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FBaseDrawPass::PrepareTargets(FRenderPipelineContext& Context)
{
    if (Context.ActiveViewSurfaceSet && Context.ViewModePassRegistry && Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        const EShadingModel ShadingModel = Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode);
        Context.ActiveViewSurfaceSet->ClearBaseTargets(Context.Context, ShadingModel);
        Context.ActiveViewSurfaceSet->BindBaseDrawTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
    }
    else
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}

void FBaseDrawPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    FMeshDrawCommandBuilder::Build(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FBaseDrawPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);
}
