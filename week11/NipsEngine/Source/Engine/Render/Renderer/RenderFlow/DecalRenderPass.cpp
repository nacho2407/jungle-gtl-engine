#include "DecalRenderPass.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"

bool FDecalRenderPass::Initialize()
{
    return true;
}

bool FDecalRenderPass::Release()
{
    return true;
}

bool FDecalRenderPass::Begin(const FRenderPassContext* Context)
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

bool FDecalRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    const FRenderBus* RenderBus = Context->RenderBus;
    const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Decal);

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

		uint32 PermutationKey = (uint32)ELightingModel::Unlit;
		switch (Context->RenderBus->GetViewMode())
		{
		case EViewMode::Lit_Gouraud:
			PermutationKey = (uint32)ELightingModel::Gouraud;
			break;
		case EViewMode::Lit_Lambert:
			PermutationKey = (uint32)ELightingModel::Lambert;
			break;
		case EViewMode::Lit_BlinnPhong:
			PermutationKey = (uint32)ELightingModel::BlinnPhong;
			break;
		}

        if (Cmd.Material)
        {
            Cmd.Material->Bind(Context->DeviceContext, PermutationKey);
        }
        CheckOverrideViewMode(Context);  
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

bool FDecalRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
