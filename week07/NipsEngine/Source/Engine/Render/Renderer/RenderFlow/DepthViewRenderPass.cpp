#include "DepthViewRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"

bool FDepthViewRenderPass::Initialize()
{
    return true;
}

bool FDepthViewRenderPass::Release()
{
    return true;
}

bool FDepthViewRenderPass::Begin(const FRenderPassContext* Context)
{
    bShouldRender = (Context->RenderBus->GetViewMode() == EViewMode::Depth);
    if (!bShouldRender)
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    if (Context->RenderTargets->SceneDepthSRV == nullptr || PrevPassRTV == nullptr)
    {
        return false;
    }

    ID3D11RenderTargetView* OutputRTV =
        Context->RenderTargets->ResolvePostProcessOutputRTV(PrevPassRTV);
    ID3D11ShaderResourceView* OutputSRV =
        Context->RenderTargets->ResolvePostProcessOutputSRV(PrevPassRTV);
    if (OutputRTV == nullptr || OutputSRV == nullptr)
    {
        return false;
    }

    ID3D11RenderTargetView* RTVs[1] = { OutputRTV };
    Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, nullptr);

    ID3D11ShaderResourceView* DepthSRV = Context->RenderTargets->SceneDepthSRV;
    Context->DeviceContext->PSSetShaderResources(0, 1, &DepthSRV);

    UShader* DepthViewShader = FResourceManager::Get().GetShader("Shaders/Multipass/DepthViewPass.hlsl");
    if (DepthViewShader == nullptr)
    {
        return false;
    }
    DepthViewShader->Bind(Context->DeviceContext);

    ID3D11RasterizerState* FullscreenRS =
        FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull, Context->Device);
    Context->DeviceContext->RSSetState(FullscreenRS);

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = OutputSRV;
    OutRTV = OutputRTV;
    return true;
}

bool FDepthViewRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (!bShouldRender)
    {
        return true;
    }

    Context->DeviceContext->Draw(3, 0);
    return true;
}

bool FDepthViewRenderPass::End(const FRenderPassContext* Context)
{
    if (!bShouldRender)
    {
        return true;
    }

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
    return true;
}
