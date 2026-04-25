#include "Render/Execute/Passes/Scene/DeferredOpaquePass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"

void FDeferredOpaquePass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(NullSRVs), NullSRVs);

    ID3D11ShaderResourceView* NullSystemSRV = nullptr;
    Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &NullSystemSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &NullSystemSRV);

    if (Context.LightCulling)
    {
        ID3D11ShaderResourceView* TileMaskSRV = Context.LightCulling->GetPerTileMaskSRV();
        Context.Context->PSSetShaderResources(ESystemTexSlot::LightTileMask, 1, &TileMaskSRV);

        ID3D11ShaderResourceView* HitMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(ESystemTexSlot::DebugHitMap, 1, &HitMapSRV);

        ID3D11Buffer* LightCullingParamsCB = Context.LightCulling->GetLightCullingParamsCB();
        Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &LightCullingParamsCB);
    }

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV    = nullptr;
        Context.StateCache->NormalSRV     = nullptr;
        Context.StateCache->SpecularSRV   = nullptr;
        Context.StateCache->LocalLightSRV = nullptr;
        Context.StateCache->bForceAll     = true;
    }
}

void FDeferredOpaquePass::PrepareTargets(FRenderPipelineContext& Context)
{
    const bool bUseViewModeSurfaces =
        Context.ViewMode.Surfaces &&
        Context.ViewMode.Registry &&
        Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode) &&
        Context.ViewMode.ActiveViewMode != EViewMode::Wireframe;

    if (bUseViewModeSurfaces)
    {
        const EShadingModel ShadingModel = Context.ViewMode.Registry->GetShadingModel(Context.ViewMode.ActiveViewMode);
        Context.ViewMode.Surfaces->ClearBaseTargets(Context.Context, ShadingModel);
        Context.ViewMode.Surfaces->BindOpaqueTargets(Context.Context, ShadingModel, Context.GetViewportDSV());
        return;
    }

    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FDeferredOpaquePass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FDeferredOpaquePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context.Context->PSSetShaderResources(ESystemTexSlot::LightTileMask, 1, &NullSRV);
    Context.Context->PSSetShaderResources(ESystemTexSlot::DebugHitMap, 1, &NullSRV);
}
