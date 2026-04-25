#include "ShadowPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"

bool FShadowPass::Initialize()
{
	return true;
}

bool FShadowPass::Begin(const FRenderPassContext* Context)
{
	return true;
}

bool FShadowPass::DrawCommand(const FRenderPassContext* Context)
{
	UShader* ShadowShader = FResourceManager::Get().GetShader("Shaders/Shadow.hlsl");
	if (ShadowShader == nullptr)
	{
		return false;
	}

	ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	const FRenderBus* RenderBus = Context->RenderBus;
	const TArray<FRenderCommand>& OpaqueCmds = RenderBus->GetCommands(ERenderPass::Opaque);

	if (RenderBus->DirLightComp == nullptr) return false;

	FConstantBuffer* ShadowBuffer = &Context->RenderResources->ShadowBuffer;

	ID3D11DepthStencilView* ShadowDSV = FShadowAtlasManager::Get().ShadowDSV.Get();
	ID3D11Texture2D* ShadowMap = FShadowAtlasManager::Get().ShadowMap.Get();
	if (ShadowDSV == nullptr || ShadowMap == nullptr)
	{
		return false;
	}

	{
        D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
        ShadowMap->GetDesc(&ShadowMapDesc);

        D3D11_VIEWPORT ShadowViewport = {};
        ShadowViewport.TopLeftX = 0.0f;
        ShadowViewport.TopLeftY = 0.0f;
        ShadowViewport.Width = static_cast<float>(ShadowMapDesc.Width);
        ShadowViewport.Height = static_cast<float>(ShadowMapDesc.Height);
        ShadowViewport.MinDepth = 0.0f;
        ShadowViewport.MaxDepth = 1.0f;

        DeviceContext->RSSetViewports(1, &ShadowViewport);
        DeviceContext->ClearDepthStencilView(ShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        DeviceContext->OMSetRenderTargets(0, nullptr, ShadowDSV);

        ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
        DeviceContext->OMSetDepthStencilState(DepthState, 0);

	}

    FShadowConstants shadowData = {};
    FMatrix CamView = RenderBus->GetView();
    FMatrix CamProj = RenderBus->GetProj();

	for (const auto& Cmd : OpaqueCmds)
	{
		if (Cmd.Type == ERenderCommandType::PostProcessOutline)
		{
			continue;
		}

		if (Cmd.MeshBuffer == nullptr || !Cmd.MeshBuffer->IsValid())
		{
			continue;
		}

		ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
		uint32 VertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
		uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
		if (VertexBuffer == nullptr || VertexCount == 0 || Stride == 0)
		{
			continue;
		}

		Context->RenderResources->PerObjectConstantBuffer.Update(DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(1, 1, &cb1);

		const UDirectionalLightComponent* DirLightComp = RenderBus->DirLightComp;

		shadowData.DirLightViewProj = DirLightComp->GetPSMMatrix(RenderBus->GetView(), RenderBus->GetProj());
		ShadowBuffer->Update(DeviceContext, &shadowData, sizeof(FShadowConstants));

		ID3D11Buffer* cb4 = ShadowBuffer->GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(4, 1, &cb4);

		uint32 Offset = 0;
		Context->DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
		ShadowShader->Bind(DeviceContext);
		CheckOverrideViewMode(Context);

		ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
		if (IndexBuffer != nullptr)
		{
			Context->DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			Context->DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
		}
		else
		{
			Context->DeviceContext->Draw(VertexCount, 0);
		}
	}

	return true;
}

bool FShadowPass::End(const FRenderPassContext* Context)
{
	Context->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	D3D11_VIEWPORT OriginViewport = {};
	OriginViewport.TopLeftX = 0.0f;
	OriginViewport.TopLeftY = 0.0f;
	OriginViewport.Width = static_cast<float>(Context->RenderBus->GetViewportSize().X);
	OriginViewport.Height = static_cast<float>(Context->RenderBus->GetViewportSize().Y);
	OriginViewport.MinDepth = 0.0f;
	OriginViewport.MaxDepth = 1.0f;

	Context->DeviceContext->RSSetViewports(1, &OriginViewport);
	return true;
}

bool FShadowPass::Release()
{
	return true;
}
