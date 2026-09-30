// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Passes/Scene/PresentPass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"

namespace
{
bool AreTexturesCopyCompatible(ID3D11Texture2D* Dest, ID3D11Texture2D* Source)
{
    if (!Dest || !Source)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC DestDesc   = {};
    D3D11_TEXTURE2D_DESC SourceDesc = {};
    Dest->GetDesc(&DestDesc);
    Source->GetDesc(&SourceDesc);

    return DestDesc.Width == SourceDesc.Width &&
           DestDesc.Height == SourceDesc.Height &&
           DestDesc.MipLevels == SourceDesc.MipLevels &&
           DestDesc.ArraySize == SourceDesc.ArraySize &&
           DestDesc.Format == SourceDesc.Format &&
           DestDesc.SampleDesc.Count == SourceDesc.SampleDesc.Count &&
           DestDesc.SampleDesc.Quality == SourceDesc.SampleDesc.Quality;
}
} // namespace

void FPresentPass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[16] = {};
    Context.Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    Context.Context->VSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
    Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

    if (Context.StateCache)
    {
        Context.StateCache->DiffuseSRV    = nullptr;
        Context.StateCache->NormalSRV     = nullptr;
        Context.StateCache->SpecularSRV   = nullptr;
        Context.StateCache->LocalLightSRV = nullptr;
        Context.StateCache->RTV           = nullptr;
        Context.StateCache->DSV           = nullptr;
        Context.StateCache->bForceAll     = true;
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

void FPresentPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
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
    ID3D11Texture2D* Dest   = Context.Device->GetFrameBufferTexture();
    if (!AreTexturesCopyCompatible(Dest, Source))
    {
        return;
    }

    Context.Context->CopyResource(Dest, Source);
}

