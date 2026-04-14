#include "PostProcessOutlineRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/Material.h"

bool FPostProcessOutlineRenderPass::Initialize()
{
    return true;
}

bool FPostProcessOutlineRenderPass::Release()
{
    return true;
}

bool FPostProcessOutlineRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, nullptr);

    UShader* OutlineShader = FResourceManager::Get().GetShader("Shaders/OutlinePostProcess.hlsl");
    if (OutlineShader != nullptr)
    {
        OutlineShader->Bind(Context->DeviceContext);
    }

    ID3D11ShaderResourceView* maskSRV = Context->RenderTargets->SelectionMaskSRV;
    Context->DeviceContext->PSSetShaderResources(7, 1, &maskSRV);

    auto DepthStencilState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
    auto BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidBackCull);

    Context->DeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    Context->DeviceContext->RSSetState(RasterizerState);

    Context->DeviceContext->IASetInputLayout(nullptr);
    Context->DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FPostProcessOutlineRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::PostProcessOutline);
    if (Commands.empty())
    {
        return true;
    }

    for (const FRenderCommand& Cmd : Commands)
    {
        if (Cmd.Material != nullptr)
        {
            Cmd.Material->Bind(Context->DeviceContext);
        }
        Context->DeviceContext->Draw(3, 0);
    }

    return true;
}

bool FPostProcessOutlineRenderPass::End(const FRenderPassContext* Context)
{
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Context->DeviceContext->PSSetShaderResources(7, 1, &nullSRV);
    return true;
}
