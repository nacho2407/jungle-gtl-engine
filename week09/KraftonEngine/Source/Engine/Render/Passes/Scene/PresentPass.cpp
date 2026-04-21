#include "Render/Passes/Scene/PresentPass.h"

#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"

void FPresentPass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[16] = {};
    Context.Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    Context.Context->VSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV = nullptr;
        Context.StateCache->NormalSRV = nullptr;
        Context.StateCache->LocalLightSRV = nullptr;
        Context.StateCache->RTV = nullptr;
        Context.StateCache->DSV = nullptr;
        Context.StateCache->bForceAll = true;
    }
}

void FPresentPass::PrepareTargets(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FPresentPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    (void)Context;
}

void FPresentPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FPresentPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.Device || !Context.Targets)
    {
        return;
    }

    ID3D11Texture2D* Source = Context.Targets->ViewportRenderTexture ? Context.Targets->ViewportRenderTexture : Context.Targets->SceneColorCopyTexture;
    ID3D11Texture2D* Dest = Context.Device->GetFrameBufferTexture();
    if (!Source || !Dest)
    {
        return;
    }

    Context.Context->CopyResource(Dest, Source);
}
