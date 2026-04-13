#include "OpaqueRenderPass.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Resource/RenderResources.h"

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

    return true;
}

bool FOpaqueRenderPass::Render(const FRenderPassContext* Context)
{
    Begin(Context);

	const FRenderBus* RenderBus = Context->RenderBus;

	const TArray<FRenderCommand>& Commands = RenderBus->GetCommands(ERenderPass::Opaque);

	if (Commands.empty())
        return true;

	ERasterizerType Rasterizer = ERasterizerType::SolidBackCull;
    const FPassRenderState* RenderState = Context->RenderState;
    
	if (RenderState->bWireframeAware && RenderBus->GetViewMode() == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerType::WireFrame;
	}

	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (RenderState->Shader)
        RenderState->Shader->Bind(Context->DeviceContext);

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

		// Draw Command (현재 테스트용으로 넣은거고 로직은 따로 뺄 예정)

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

	End(Context);

    return true;
}

bool FOpaqueRenderPass::End(const FRenderPassContext* Context)
{
	// 스마트 객체로 처리하면 좀 더 좋을 거 같은데
	// 썼던 자원은 바인딩 해제해주는 것이 이후 스테이지 오염을 막을 수 있음
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    return true;
}

bool FOpaqueRenderPass::Release()
{
    return true;
}
