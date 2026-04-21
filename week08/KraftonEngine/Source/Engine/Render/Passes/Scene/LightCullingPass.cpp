#include "LightCullingPass.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Pipelines/Context/FrameSharedResources.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Visibility/TileBasedLightCulling.h"

void FLightCullingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    // RTV 해제 (CS 단계)
    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    // Depth 복사 후 CS t1에 바인딩
    if (Context.Targets->DepthTexture && Context.Targets->DepthCopyTexture)
        Context.Context->CopyResource(Context.Targets->DepthCopyTexture, Context.Targets->DepthTexture);

    if (Context.GetViewportDSV())
    {
        Context.Context->CSSetShaderResources(1, 1, &Context.Targets->DepthCopySRV);
    }
    // LocalLight SRV CS t6에 바인딩
    if (Context.Resources && Context.Resources->LocalLightSRV)
        Context.Context->CSSetShaderResources(6, 1, &Context.Resources->LocalLightSRV);
}

void FLightCullingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (Context.SceneView->ViewMode == EViewMode::Wireframe)
        return;

    if (Context.LightCulling && Context.Resources->LocalLightCapacity > 0)
    {
        Context.LightCulling->SetPointLightData(Context.Resources->LocalLightCount);
        Context.LightCulling->Dispatch(*Context.SceneView, true);
    }

    // CS SRV 해제
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    Context.Context->CSSetShaderResources(1, 1, &NullSRVs[0]);
    Context.Context->CSSetShaderResources(6, 1, &NullSRVs[1]);
}