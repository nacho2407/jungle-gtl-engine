#include "Render/Passes/Scene/LightingPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"
#include "Render/Pipelines/Context/ViewMode/SceneViewModeSurfaces.h"
#include "Render/Pipelines/Registry/ViewModePassRegistry.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/RenderResources.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Pipelines/Context/FrameRenderResources.h"
#include "Render/Visibility/TileBasedLightCulling.h"

namespace
{
void BuildSurfaceSRVTable(const FRenderPipelineContext& Context, EShadingModel ShadingModel, ID3D11ShaderResourceView* OutSurfaceSRVs[6])
{
    for (uint32 i = 0; i < 6; ++i)
    {
        OutSurfaceSRVs[i] = nullptr;
    }

    if (!Context.ActiveViewSurfaces)
    {
        return;
    }

    OutSurfaceSRVs[0] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::BaseColor);
    OutSurfaceSRVs[3] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedBaseColor);

    switch (ShadingModel)
    {
    case EShadingModel::Gouraud:
        // Surface1 = GouraudL, 나머지 표면은 사용하지 않습니다.
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        break;

    case EShadingModel::Lambert:
        // Surface1 = Normal, ModifiedSurface1 = Decal 적용 Normal
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        OutSurfaceSRVs[4] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface1);
        break;

    case EShadingModel::BlinnPhong:
        // Surface1 = Normal, Surface2 = MaterialParam
        OutSurfaceSRVs[1] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface1);
        OutSurfaceSRVs[2] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::Surface2);
        OutSurfaceSRVs[4] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface1);
        OutSurfaceSRVs[5] = Context.ActiveViewSurfaces->GetSRV(ESceneViewModeSurfaceSlot::ModifiedSurface2);
        break;

    case EShadingModel::Unlit:
    default:
        break;
    }
}
} // namespace

void FLightingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.ActiveViewSurfaces || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    const EShadingModel ShadingModel = Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode);
    if (ShadingModel == EShadingModel::Unlit)
    {
        return;
    }

    const bool bNeedsReadableDepth = Targets && Targets->DepthTexture && Targets->DepthCopyTexture &&
                                     Targets->DepthTexture != Targets->DepthCopyTexture;

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (bNeedsReadableDepth)
    {
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    ID3D11ShaderResourceView* SurfaceSRVs[6] = {};
    BuildSurfaceSRVTable(Context, ShadingModel, SurfaceSRVs);
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(SurfaceSRVs), SurfaceSRVs);

	// LightCulling 관련 데이터 바이딩
    if (Context.LightCulling)
    {
        // TileMask SRV 추가
        ID3D11ShaderResourceView* TileMaskSRV = Context.LightCulling->GetPerTileMaskSRV();
        Context.Context->PSSetShaderResources(7, 1, &TileMaskSRV);

		// 디버그 용 히트맵 SRV 추가
		ID3D11ShaderResourceView* HipMapSRV = Context.LightCulling->GetDebugHitMapSRV();
        Context.Context->PSSetShaderResources(8, 1, &HipMapSRV);

		//b2 LightCullingParams
		ID3D11Buffer* LightCullingParamsCB = Context.LightCulling->GetLightCullingParamsCB();
        Context.Context->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &LightCullingParamsCB);
    }

    if (Targets && Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    if (Context.Resources)
    {
        ID3D11Buffer* GlobalLightBuffer = Context.Resources->GlobalLightBuffer.GetBuffer();
        Context.Context->PSSetConstantBuffers(ECBSlot::Light, 1, &GlobalLightBuffer);

        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }

    if (Context.StateCache)
    {
        Context.StateCache->LightCB = Context.Resources ? &Context.Resources->GlobalLightBuffer : nullptr;
        Context.StateCache->LocalLightSRV = Context.Resources ? Context.Resources->LocalLightSRV : nullptr;
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->SpecularSRV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FLightingPass::PrepareTargets(FRenderPipelineContext& Context)
{
    BindViewportTarget(Context);
}

void FLightingPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.ActiveViewSurfaces || !Context.ViewModePassRegistry || !Context.ViewModePassRegistry->HasConfig(Context.ActiveViewMode))
    {
        return;
    }

    if (Context.ViewModePassRegistry->GetShadingModel(Context.ActiveViewMode) == EShadingModel::Unlit)
    {
        return;
    }

    DrawCommandBuilder::BuildFullscreenDrawCommand(ERenderPass::Lighting, Context, *Context.DrawCommandList);

    if (!Context.DrawCommandList || Context.DrawCommandList->GetCommands().empty())
    {
        return;
    }

    FDrawCommand& Command = Context.DrawCommandList->GetCommands().back();
    Command.PerShaderCB[0] = Context.LightCulling ? Context.LightCulling->GetLightCullingParamsCBWrapper() : nullptr;
}

void FLightingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!Context.DrawCommandList)
    {
        return;
    }

    SubmitPassRange(Context, ERenderPass::Lighting);

    if (Targets && Targets->ViewportRenderTexture && Targets->SceneColorCopyTexture &&
        Targets->ViewportRenderTexture != Targets->SceneColorCopyTexture)
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);

        BindViewportTarget(Context);
    }

	ID3D11ShaderResourceView* nullSRV = {};
    Context.Context->PSSetShaderResources(7, 1, &nullSRV);

    Context.Context->PSSetShaderResources(8, 1, &nullSRV);
}
