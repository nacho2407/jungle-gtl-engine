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

	FConstantBuffer* ShadowBuffer = &Context->RenderResources->ShadowBuffer;
	FShadowAtlasManager& ShadowAtlasManager = FShadowAtlasManager::Get();

	if (RenderBus->ShadowLightRequests.empty())
	{
		return true;
	}

	ID3D11DepthStencilView* ShadowDSV = ShadowAtlasManager.ShadowMapAtlas.ShadowDSV.Get();
	ID3D11Texture2D* ShadowMap = ShadowAtlasManager.ShadowMapAtlas.ShadowMap.Get();
	if (ShadowDSV == nullptr || ShadowMap == nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	ShadowMap->GetDesc(&Desc);

	const int32 TileSize = static_cast<int32>(ShadowAtlasManager.GetTileSize());
	const float AtlasW = static_cast<float>(Desc.Width);
	const float AtlasH = static_cast<float>(Desc.Height);
	const float TileSizeF = static_cast<float>(TileSize);

	ID3D11DepthStencilState* DepthState =
		FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);

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

		int32 TileX, TileY;
		if (!ShadowAtlasManager.AllocateTile(TileX, TileY))
		{
			continue;
		}
		ShadowAtlasManager.FreeTile(TileX, TileY); // TODO: 생명주기 연동

		// 타일 뷰포트
		D3D11_VIEWPORT VP = {};
		VP.TopLeftX = static_cast<float>(TileX * TileSize);
		VP.TopLeftY = static_cast<float>(TileY * TileSize);
		VP.Width = TileSizeF;
		VP.Height = TileSizeF;
		VP.MinDepth = 0.0f;
		VP.MaxDepth = 1.0f;
		DeviceContext->RSSetViewports(1, &VP);
		DeviceContext->ClearDepthStencilView(ShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
		DeviceContext->OMSetRenderTargets(0, nullptr, ShadowDSV);
		DeviceContext->OMSetDepthStencilState(DepthState, 0);

		const uint32 ShadowKey = static_cast<uint32>(LightComp->GetShadowMapType());
		FShadowConstants ShadowData = {};
		switch (Request.Type)
		{
		case EShadowLightType::SLT_Directional:
		{
			ShadowData.DirLightViewProj = LightComp->GetLightViewProj(RenderBus->GetView(), RenderBus->GetProj());
			break;
		}

		// TODO : POINT 와 SpotLight 
		default:
			break;
		}

		ShadowData.ScaleOffset = FVector4(
			TileSizeF / AtlasW, TileSizeF / AtlasH,
			(TileX * TileSizeF) / AtlasW, (TileY * TileSizeF) / AtlasH);

		// 상수 버퍼 & 셰이더는 라이트당 1회
		ShadowBuffer->Update(DeviceContext, &ShadowData, sizeof(FShadowConstants));
		ID3D11Buffer* cb4 = ShadowBuffer->GetBuffer();
		DeviceContext->VSSetConstantBuffers(4, 1, &cb4);
		ShadowShader->Bind(DeviceContext, ShadowKey);

		// 메시 별 순회
		for (const auto& Cmd : OpaqueCmds)
		{
			if (Cmd.Type == ERenderCommandType::PostProcessOutline) continue;
			if (!Cmd.MeshBuffer || !Cmd.MeshBuffer->IsValid())      continue;

			ID3D11Buffer* VB = Cmd.MeshBuffer->GetVertexBuffer().GetBuffer();
			uint32        VCount = Cmd.MeshBuffer->GetVertexBuffer().GetVertexCount();
			uint32        Stride = Cmd.MeshBuffer->GetVertexBuffer().GetStride();
			if (!VB || VCount == 0 || Stride == 0) continue;

			Context->RenderResources->PerObjectConstantBuffer.Update(
				DeviceContext, &Cmd.PerObjectConstants, sizeof(FPerObjectConstants));
			ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
			DeviceContext->VSSetConstantBuffers(1, 1, &cb1);

			uint32 Offset = 0;
			DeviceContext->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
			CheckOverrideViewMode(Context);

			ID3D11Buffer* IB = Cmd.MeshBuffer->GetIndexBuffer().GetBuffer();
			if (IB)
			{
				DeviceContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
				DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
			}
			else
			{
				DeviceContext->Draw(VCount, 0);
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
