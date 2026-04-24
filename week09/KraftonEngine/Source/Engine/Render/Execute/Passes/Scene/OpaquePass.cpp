// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Passes/Scene/OpaquePass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Visibility/LightCulling/TileBasedLightCulling.h"
void FOpaquePass::PrepareInputs(FRenderPipelineContext& Context)
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
        Context.Context->PSSetShaderResources(7, 1, &TileMaskSRV);

        // Deq
        ID3D11ShaderResourceView* HipMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(8, 1, &HipMapSRV);

        // b2 LightCullingParams
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

void FOpaquePass::PrepareTargets(FRenderPipelineContext& Context)
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
    }
    else
    {
        ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
        Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
    }
}

void FOpaquePass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FOpaquePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);

    ID3D11ShaderResourceView* nullSRV = {};
    Context.Context->PSSetShaderResources(7, 1, &nullSRV);

    Context.Context->PSSetShaderResources(8, 1, &nullSRV);
}
