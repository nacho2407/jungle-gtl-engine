#include "OpaqueRenderPass.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Core/ResourceManager.h"

bool FOpaqueRenderPass::Initialize()
{
    return true;
}

bool FOpaqueRenderPass::Begin(const FRenderPassContext* Context)
{
    const FRenderTargetSet* RenderTargets = Context->RenderTargets;
    ID3D11RenderTargetView* RTVs[3] = { 
		RenderTargets->SceneColorRTV, 
		RenderTargets->SceneNormalRTV,
		RenderTargets->SceneWorldPosRTV
	};
    ID3D11DepthStencilView* DSV = RenderTargets->DepthStencilView;

	Context->DeviceContext->OMSetRenderTargets(ARRAYSIZE(RTVs), RTVs, DSV);
    OutSRV = RenderTargets->SceneColorSRV;
    OutRTV = RenderTargets->SceneColorRTV;

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FOpaqueRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const FRenderBus* RenderBus = Context->RenderBus;

    const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Opaque);

    if (Commands.empty())
        return true;

    for (const FRenderCommand& Cmd : Commands)
    {
        Context->RenderResources->PerObjectConstantBuffer.Update(Context->DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
        ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
        Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
        Context->DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

        if (Cmd.Type == ERenderCommandType::PostProcessOutline)
        {
            continue;
        }

        if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
        {
            return false;
        }

        uint32 offset = 0;
        ID3D11Buffer* vertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
        if (vertexBuffer == nullptr)
        {
            return false;
        }

        uint32 vertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
        uint32 stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
        if (vertexCount == 0 || stride == 0)
        {
            return false;
        }

        if (Cmd.Material)
        {
            Cmd.Material->Bind(Context->DeviceContext);
        }

        Context->DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        ID3D11Buffer* indexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
        if (indexBuffer != nullptr)
        {
            uint32 indexStart = Cmd.SectionIndexStart;
            uint32 indexCount = Cmd.SectionIndexCount;
            Context->DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            Context->DeviceContext->DrawIndexed(indexCount, indexStart, 0);
        }
        else
        {
            Context->DeviceContext->Draw(vertexCount, 0);
        }
    }

    return true;
}

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}

bool FOpaqueRenderPass::Release()
{
    return true;
}
