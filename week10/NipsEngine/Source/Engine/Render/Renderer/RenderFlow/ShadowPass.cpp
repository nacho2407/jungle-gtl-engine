#include "ShadowPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Component/PostProcess/Light/LightComponent.h"

bool FShadowPass::Initialize()
{
	return true;
}

bool FShadowPass::Begin(const FRenderPassContext* Context)
{
	FShadowAtlasManager::Get().ClearTiles();

	ID3D11DepthStencilView* ShadowDSV = FShadowAtlasManager::Get().GetDSV();
	if (ShadowDSV != nullptr)
	{
		Context->DeviceContext->ClearDepthStencilView(
			ShadowDSV,
			D3D11_CLEAR_DEPTH,
			1.0f,
			0);
	}
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
	if (RenderBus->ShadowLightRequests.empty())
	{
		return true;
	}

	FConstantBuffer* ShadowBuffer = &Context->RenderResources->ShadowBuffer;
	FShadowAtlasManager& ShadowAtlasManager = FShadowAtlasManager::Get();

	ID3D11DepthStencilView* ShadowDSV = ShadowAtlasManager.GetDSV();
	if (ShadowDSV == nullptr || ShadowAtlasManager.GetAtlas() == nullptr)
	{
		return false;
	}

	ID3D11DepthStencilState* DepthState =
		FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);

	TArray<FBoundingBox> VisibleBounds;
	VisibleBounds.reserve(OpaqueCmds.size());
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

		VisibleBounds.push_back(Cmd.WorldAABB);
	}

	for (const FShadowLightRequest& Request : RenderBus->ShadowLightRequests)
	{
		if (!Request.bCastShadows || !Request.LightComponent)
		{
			continue;
		}

		ULightComponent* LightComp = Cast<ULightComponent>(Request.LightComponent);
		if (!LightComp)
		{
			continue;
		}

		if (Request.Type != EShadowLightType::SLT_Directional)
		{
			continue;
		}

		FShadowAtlasTile ShadowTile;
		if (!ShadowAtlasManager.AllocateTile(ShadowTile))
		{
			continue;
		}

		D3D11_VIEWPORT ShadowViewport = {};
		ShadowViewport.TopLeftX = static_cast<float>(ShadowTile.PixelX);
		ShadowViewport.TopLeftY = static_cast<float>(ShadowTile.PixelY);
		ShadowViewport.Width = static_cast<float>(ShadowTile.Width);
		ShadowViewport.Height = static_cast<float>(ShadowTile.Height);
		ShadowViewport.MinDepth = 0.0f;
		ShadowViewport.MaxDepth = 1.0f;
		DeviceContext->RSSetViewports(1, &ShadowViewport);
		DeviceContext->OMSetRenderTargets(0, nullptr, ShadowDSV);
		DeviceContext->OMSetDepthStencilState(DepthState, 0);

		const uint32 ShadowKey = static_cast<uint32>(LightComp->GetShadowMapType());
		FShadowConstants ShadowData = {};
		ShadowData.VirtualViewProj = RenderBus->GetView() * RenderBus->GetProj();
		ShadowData.DirLightViewProj = LightComp->GetLightViewProj(
			RenderBus->GetView(),
			RenderBus->GetProj(),
			&VisibleBounds);
		ShadowData.ScaleOffset = ShadowTile.ScaleOffset;

		ShadowBuffer->Update(DeviceContext, &ShadowData, sizeof(FShadowConstants));
		ID3D11Buffer* cb4 = ShadowBuffer->GetBuffer();
		DeviceContext->VSSetConstantBuffers(4, 1, &cb4);
		DeviceContext->PSSetConstantBuffers(4, 1, &cb4);
		ShadowShader->Bind(DeviceContext, ShadowKey);

		for (const auto& Cmd : OpaqueCmds)
		{
			if (Cmd.Type == ERenderCommandType::PostProcessOutline)
			{
				continue;
			}
			if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid())
			{
				continue;
			}

			ID3D11Buffer* VertexBuffer = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
			uint32 VertexCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
			uint32 Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
			if (!VertexBuffer || VertexCount == 0 || Stride == 0)
			{
				continue;
			}

			Context->RenderResources->PerObjectConstantBuffer.Update(
				DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
			ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
			DeviceContext->VSSetConstantBuffers(1, 1, &cb1);

			uint32 Offset = 0;
			DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
			CheckOverrideViewMode(Context);

			ID3D11Buffer* IndexBuffer = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
			if (IndexBuffer)
			{
				DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
			}
			else
			{
				DeviceContext->Draw(VertexCount, 0);
			}
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
