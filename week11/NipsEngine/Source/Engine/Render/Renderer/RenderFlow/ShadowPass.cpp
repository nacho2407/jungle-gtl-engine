#include "ShadowPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Component/PostProcess/Light/LightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"

#include <algorithm>
namespace
{
	const ULightComponent* GetSupportedShadowLight(const FShadowLightRequest& Request)
	{
		if (!Request.bCastShadows)
		{
			return nullptr;
		}

		if (Request.Type != EShadowLightType::SLT_Directional &&
			Request.Type != EShadowLightType::SLT_Spot)
		{
			return nullptr;
		}

		return Cast<ULightComponent>(Request.LightComponent);
	}
}

bool FShadowPass::Initialize()
{
	return true;
}

bool FShadowPass::Begin(const FRenderPassContext* Context)
{
	FShadowAtlasManager::Get().ClearTiles();
	ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
	Context->DeviceContext->PSSetShaderResources(10, 1, NullSRV);

	// Shadow Atlas에 쓰기

	ID3D11DepthStencilView* ShadowDSV = FShadowAtlasManager::Get().GetDSV();
	if (ShadowDSV != nullptr)
	{
		Context->DeviceContext->OMSetRenderTargets(0, nullptr, ShadowDSV);
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

	ShadowShader->Bind(Context->DeviceContext);

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

	D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
	ShadowAtlasManager.GetAtlas()->GetDesc(&ShadowMapDesc);

	FMatrix CamView = RenderBus->GetView();
	FMatrix CamProj = RenderBus->GetProj();

	TArray<FBoundingBox> VisibleBounds;
	for (const auto& Cmd : OpaqueCmds)
	{
		VisibleBounds.push_back(Cmd.WorldAABB);
	}

	TArray<uint32> LightShadowIndices(RenderBus->LightInfos.size(), InvalidShadowIndex);
	TArray<FShadowAtlasConstants> ShadowAtlasConstants;
	ShadowAtlasConstants.reserve(RenderBus->ShadowLightRequests.size());

	FShadowConstants DirectionalShadowData = {};
	bool bHasDirectionalShadow = false;

	TArray<FShadowLightRequest> SortedRequests = RenderBus->ShadowLightRequests;
    std::sort(SortedRequests.begin(), SortedRequests.end(), [](const FShadowLightRequest& A, const FShadowLightRequest& B)
              { 
			  if(A.Type != B.Type)
			  {
				  return A.Type > B.Type;
			  }
			  return A.ShadowResolution > B.ShadowResolution; });

	for (const FShadowLightRequest& Request : SortedRequests)
	{
		const ULightComponent* LightComp = GetSupportedShadowLight(Request);

		if (LightComp == nullptr)
		{
			continue;
		}
		if (!Request.bCastShadows || !Request.LightComponent)
		{
			continue;
		}

		FShadowAtlasTile ShadowTile;
		if (!FShadowAtlasManager::Get().AllocateTile(Request.ShadowResolution, ShadowTile))
		{
			// atlas가 꽉 찼음
			// shadow 해상도 낮추기, 해당 light shadow skip, atlas resize 등 처리 필요
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

		// Debug용으로 라이트 컴포넌트에 타일 오프셋 정보 전달 (실제 렌더링에는 사용되지 않음)
        {
            ULightComponent* MutableLight = const_cast<ULightComponent*>(LightComp);
            MutableLight->DebugShadowAtlasScaleOffset = ShadowTile.ScaleOffset;
            MutableLight->bHasDebugShadowAtlasTile = true;
        }

        ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
        DeviceContext->OMSetDepthStencilState(DepthState, 0);

		const uint32 ShadowKey = static_cast<uint32>(LightComp->GetShadowMapType());
		FShadowConstants ShadowData = {};

		FBoundingBox VisibleBoundingBox;

		for (const auto& Box : VisibleBounds)
		{
			VisibleBoundingBox.Merge(Box);
		}

		float Slideback = 1.0f;

		float Near = RenderBus->GetNearPlane();
		float MinZ = FLT_MAX;
		FVector Corners[8];
		VisibleBoundingBox.GetVertices(Corners);
		for (int i = 0; i < 8; ++i)
		{
			auto ToBB = Corners[i] - RenderBus->GetCameraPosition();
			float X = FVector::DotProduct(ToBB, RenderBus->GetCameraForward());

			MinZ = std::min(X, MinZ);
		}

		if (MinZ < FLT_MAX && MinZ > Near)
		{
			Near = MinZ;
		}

		if (LightComp->GetShadowMapType() == EShadowMap::PSM)
		{
			FVector PrevCameraUp = RenderBus->GetCameraUp();
			FVector VirtualCamPos = RenderBus->GetCameraPosition() - RenderBus->GetCameraForward() * Slideback;
			FVector VirtualCamTarget = RenderBus->GetCameraPosition() + RenderBus->GetCameraForward() * Slideback;
			FVector VirtualCamUp = VirtualCamPos + PrevCameraUp;

			FMatrix VirtualView = FMatrix::MakeViewLookAtLH(VirtualCamPos, VirtualCamTarget, VirtualCamUp);
			FMatrix VirtualProj = FMatrix::MakePerspectiveFovLH(MathUtil::DegreesToRadians(90.0f), 1.0f, Near + Slideback, RenderBus->GetFarPlane());

			ShadowData.VirtualViewProj = VirtualView * VirtualProj;
			ShadowData.DirLightViewProj = LightComp->GetLightViewProj(
				VirtualView,
				VirtualProj,
				&VisibleBounds);
		}
		else
		{
			ShadowData.VirtualViewProj = CamView * CamProj;
			ShadowData.DirLightViewProj = LightComp->GetLightViewProj(
				CamView,
				CamProj,
				&VisibleBounds);
		}

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

		uint32 ShadowIndex = static_cast<uint32>(ShadowAtlasConstants.size());
		if (Request.LightIndex != InvalidShadowIndex && Request.LightIndex < LightShadowIndices.size())
		{
			LightShadowIndices[Request.LightIndex] = ShadowIndex;
		}

		float tile = static_cast<float>(ShadowTile.Width);

		FShadowAtlasConstants atlasConstants = {};
		atlasConstants.ShadowViewProjMatrix = ShadowData.DirLightViewProj;
		atlasConstants.ScaleOffset = ShadowTile.ScaleOffset;
		atlasConstants.ShadowBias = Request.ShadowBias;
		atlasConstants.ShadowStrength = 1.0f;
		atlasConstants.ShadowSoftness = Request.ShadowSharpen;
		atlasConstants.ShadowType = static_cast<uint32>(Request.Type);
		ShadowAtlasConstants.push_back(atlasConstants);

		if (Request.Type == EShadowLightType::SLT_Directional)
		{
			DirectionalShadowData.DirLightViewProj = ShadowData.DirLightViewProj;
			DirectionalShadowData.VirtualViewProj = ShadowData.VirtualViewProj;
			DirectionalShadowData.ScaleOffset = atlasConstants.ScaleOffset;
			bHasDirectionalShadow = true;
		}
	}

	if (!LightShadowIndices.empty())
	{
		Context->RenderResources->LightShadowIndexBuffer.Update(
			DeviceContext,
			LightShadowIndices.data(),
			static_cast<uint32>(LightShadowIndices.size()));
	}

	if (!ShadowAtlasConstants.empty())
	{
		Context->RenderResources->AtlasShadowBuffer.Update(
			DeviceContext,
			ShadowAtlasConstants.data(),
			static_cast<uint32>(ShadowAtlasConstants.size()));
	}

	if (bHasDirectionalShadow)
	{
		ShadowBuffer->Update(DeviceContext, &DirectionalShadowData, sizeof(FShadowConstants));
		ID3D11Buffer* cb4 = ShadowBuffer->GetBuffer();
		Context->DeviceContext->VSSetConstantBuffers(4, 1, &cb4);
		Context->DeviceContext->PSSetConstantBuffers(4, 1, &cb4);
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

void FShadowPass::DrawShadowCaster(const FRenderPassContext* Context, const ULightComponent* Light)
{
    
}

void FShadowPass::DrawShadowCubeCaster(const FRenderPassContext* Context, const ULightComponent* Light)
{
    FShadowAtlasManager& Shadow = FShadowAtlasManager::Get();
    int32 ShadowCubeIndex;
    if (Shadow.AllocateTileCube(ShadowCubeIndex)) return;

    for (uint32 Face = 0; Face < 6; ++Face)
    {
        ID3D11DepthStencilView* DSV =
            Shadow.GetCubeDSV(ShadowCubeIndex, Face);

        Context->DeviceContext->OMSetRenderTargets(0, nullptr, DSV);
        Context->DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // face별 ViewProj 세팅 후 shadow caster 렌더링

    }
}

bool FShadowPass::Release()
{
	return true;
}
