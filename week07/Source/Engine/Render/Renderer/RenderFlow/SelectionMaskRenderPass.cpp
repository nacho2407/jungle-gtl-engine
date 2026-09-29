#include "SelectionMaskRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"

bool FSelectionMaskRenderPass::Initialize()
{
    return true;
}

bool FSelectionMaskRenderPass::Release()
{
    return true;
}

bool FSelectionMaskRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = Context->RenderTargets->SelectionMaskRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto DepthStencilState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::StencilWrite);
    auto BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    Context->DeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
    Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    Context->DeviceContext->RSSetState(RasterizerState);

    UShader* SelectionMaskShader = FResourceManager::Get().GetShader("Shaders/SelectionMask.hlsl");
    if (SelectionMaskShader != nullptr)
    {
        SelectionMaskShader->Bind(Context->DeviceContext);
    }

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FSelectionMaskRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::SelectionMask);
    if (Commands.empty())
    {
        return true;
    }

    for (const FRenderCommand& Cmd : Commands)
    {
        Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
        ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
        Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
        Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

        if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
        {
            continue;
        }

        uint32 offset = 0;
        ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
        if (vertexBuffer == nullptr)
        {
            continue;
        }

        uint32 vertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
        uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
        if (vertexCount == 0 || stride == 0)
        {
            continue;
        }

        Context->DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
        if (indexBuffer != nullptr)
        {
            Context->DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            Context->DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
        }
        else
        {
            Context->DeviceContext->Draw(vertexCount, 0);
        }
    }

    return true;
}

bool FSelectionMaskRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
