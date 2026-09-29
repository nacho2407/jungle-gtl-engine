#include "TranslucentRenderPass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"

namespace
{
    bool DrawMeshCommandsForPass(const FRenderPassContext* Context, ERenderPass Pass)
    {
        const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(Pass);
        if (Commands.empty())
        {
            return true;
        }

        for (const FRenderCommand& Cmd : Commands)
        {
            Context->RenderResources->PerObjectConstantBuffer.Update(
                Context->DeviceContext,
                &Cmd.PerObjectConstants,
                sizeof(FPerObjectConstants));
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

            if (Cmd.Material != nullptr)
            {
                Cmd.Material->Bind(Context->DeviceContext);
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
}

bool FTranslucentRenderPass::Initialize()
{
    return true;
}

bool FTranslucentRenderPass::Release()
{
    return true;
}

bool FTranslucentRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
    Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;

    return true;
}

bool FTranslucentRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    return DrawMeshCommandsForPass(Context, ERenderPass::Translucent);
}

bool FTranslucentRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
