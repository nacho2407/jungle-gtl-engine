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

	if (RenderBus->DirectionalLightShadow.LightComponent == nullptr) return false;

	FConstantBuffer* ShadowBuffer = &Context->RenderResources->ShadowBuffer;

	ID3D11DepthStencilView* ShadowDSV = FShadowAtlasManager::Get().ShadowMapAtlas.ShadowDSV.Get();
	ID3D11Texture2D* ShadowMap = FShadowAtlasManager::Get().ShadowMapAtlas.ShadowMap.Get();
	if (ShadowDSV == nullptr || ShadowMap == nullptr)
	{
		return false;
	}

	
	int32 tileX, tileY;
    int32 tileSize = FShadowAtlasManager::Get().GetTileSize();

    if (!FShadowAtlasManager::Get().AllocateTile(tileX, tileY))
    {
        // atlas가 꽉 찼음
        // shadow 해상도 낮추기, 해당 light shadow skip, atlas resize 등 처리 필요
    }
    // TODO:used Tile FreeTile to mark the tile as unused, but it doesn't actually clear the tile in the shadow map atlas.
    FShadowAtlasManager::Get().FreeTile(tileX, tileY);

    D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
	{
        ShadowMap->GetDesc(&ShadowMapDesc);

        D3D11_VIEWPORT ShadowViewport = {};
        ShadowViewport.TopLeftX = static_cast<float>(tileX * tileSize);
        ShadowViewport.TopLeftY = static_cast<float>(tileY * tileSize);
        ShadowViewport.Width = static_cast<float>(tileSize);
        ShadowViewport.Height = static_cast<float>(tileSize);
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

	const UDirectionalLightComponent* DirLightComp =
		Cast<UDirectionalLightComponent>(RenderBus->DirectionalLightShadow.LightComponent);

	TArray<FBoundingBox> VisibleBounds;
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

	shadowData.VirtualViewProj = CamView * CamProj;
	shadowData.DirLightViewProj = DirLightComp->GetPSMMatrix(CamView, CamProj, VisibleBounds);

	float atlasW = static_cast<float>(ShadowMapDesc.Width);
	float atlasH = static_cast<float>(ShadowMapDesc.Height);
	float tile = static_cast<float>(tileSize);

	shadowData.ScaleOffset = FVector4(
		tile / atlasW,
		tile / atlasH,
		(tileX * tile) / atlasW,
		(tileY * tile) / atlasH);

	ShadowBuffer->Update(DeviceContext, &shadowData, sizeof(FShadowConstants));

	ID3D11Buffer* cb4 = ShadowBuffer->GetBuffer();
	Context->DeviceContext->VSSetConstantBuffers(4, 1, &cb4);

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
