#include "LightCullingPass.h"

#include "Render/Pipelines/Context/FrameRenderResources.h"
#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Visibility/TileBasedLightCulling.h"

void FLightCullingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Context || !Context.Targets)
    {
        return;
    }

    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (Context.Targets->DepthTexture && Context.Targets->DepthCopyTexture)
    {
        Context.Context->CopyResource(Context.Targets->DepthCopyTexture, Context.Targets->DepthTexture);
    }

    if (Context.Targets->DepthCopySRV)
    {
        ID3D11ShaderResourceView* DepthSRV = Context.Targets->DepthCopySRV;
        Context.Context->CSSetShaderResources(1, 1, &DepthSRV);
    }

    if (Context.Resources && Context.Resources->LocalLightSRV)
    {
        ID3D11ShaderResourceView* LocalLightsSRV = Context.Resources->LocalLightSRV;
        Context.Context->CSSetShaderResources(ESystemTexSlot::LocalLights, 1, &LocalLightsSRV);
    }
}

void FLightCullingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.Context || !Context.SceneView)
    {
        return;
    }

    if (Context.SceneView->ViewMode == EViewMode::Wireframe)
    {
        if (Context.LightCulling)
        {
            Context.LightCulling->ClearDebugHitMap();
        }
        return;
    }

    if (Context.LightCulling && Context.Resources && Context.Resources->LocalLightCount >= 0)
    {
		//Frame Buffer 할당
        ID3D11Buffer* b0 = Context.Resources->FrameBuffer.GetBuffer();
        Context.Context->CSSetConstantBuffers(ECBSlot::Frame, 1, &b0);

        Context.LightCulling->SetPointLightData(Context.Resources->LocalLightCount);
        Context.LightCulling->Dispatch(*Context.SceneView, true);
    }

    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    Context.Context->CSSetShaderResources(1, 1, &NullSRVs[0]);
    Context.Context->CSSetShaderResources(ESystemTexSlot::LocalLights, 1, &NullSRVs[1]);
}
