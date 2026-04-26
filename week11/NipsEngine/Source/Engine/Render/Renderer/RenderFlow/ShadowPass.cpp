#include "ShadowPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Component/PostProcess/Light/LightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr uint32 CascadeCount = 4;

	void SetCascadeSplitFar(FVector4& OutValue, uint32 CascadeIndex, float SplitFar)
	{
		switch (CascadeIndex)
		{
		case 0: OutValue.X = SplitFar; break;
		case 1: OutValue.Y = SplitFar; break;
		case 2: OutValue.Z = SplitFar; break;
		case 3: OutValue.W = SplitFar; break;
		default: break;
		}
	}

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

void FShadowPass::RenderShadowDepth(
	const FRenderPassContext* Context,
	FConstantBuffer* ShadowBuffer,
	UShader* ShadowShader,
	const TArray<FRenderCommand>& OpaqueCmds,
	ID3D11DepthStencilView* ShadowDSV,
	const D3D11_VIEWPORT& ShadowViewport,
	uint32 ShadowKey,
	const FShadowConstants& ShadowData)
{
	ID3D11DeviceContext* DeviceContext = Context->DeviceContext;

	DeviceContext->RSSetViewports(1, &ShadowViewport);
	DeviceContext->OMSetRenderTargets(0, nullptr, ShadowDSV);

	ID3D11DepthStencilState* DepthState =
		FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
	DeviceContext->OMSetDepthStencilState(DepthState, 0);

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
			DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			DeviceContext->DrawIndexed(Cmd.SectionIndexCount, Cmd.SectionIndexStart, 0);
		}
		else
		{
			DeviceContext->Draw(VertexCount, 0);
		}
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

	TArray<uint32> LightShadowIndices(RenderBus->LightInfos.size(), InvalidShadowIndex);
	TArray<FShadowAtlasConstants> ShadowAtlasConstants;
	ShadowAtlasConstants.reserve(RenderBus->ShadowLightRequests.size());

	float atlasW = static_cast<float>(ShadowMapDesc.Width);
	float atlasH = static_cast<float>(ShadowMapDesc.Height);
	FShadowConstants DirectionalShadowData = {};
	bool bHasDirectionalShadow = false;

	for (const FShadowLightRequest& Request : RenderBus->ShadowLightRequests)
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

		const uint32 ShadowKey = static_cast<uint32>(LightComp->GetShadowMapType());

		if (Request.Type == EShadowLightType::SLT_Directional &&
			LightComp->GetShadowMapType() == EShadowMap::CSM)
		{
			DirectionalShadowData.VirtualViewProj = CamView * CamProj;
			DirectionalShadowData.DirectionalCascadeCount = 0;

			FCascadeSplit CascadeSplits[4] = {};

			// MaxDistance 하드 코딩
			BuildPracticalCascadeSplit(
				RenderBus->GetNearPlane(),
				RenderBus->GetFarPlane(),
				100.f, 0.75f,
				CascadeSplits);

			for (uint32 CascadeIndex = 0; CascadeIndex < 4; ++CascadeIndex)
			{
				FShadowAtlasTile ShadowTile;
				if (!ShadowAtlasManager.AllocateTile(ShadowTile))
				{
					break;
				}

				D3D11_VIEWPORT ShadowViewport = {};
				ShadowViewport.TopLeftX = static_cast<float>(ShadowTile.PixelX);
				ShadowViewport.TopLeftY = static_cast<float>(ShadowTile.PixelY);
				ShadowViewport.Width = static_cast<float>(ShadowTile.Width);
				ShadowViewport.Height = static_cast<float>(ShadowTile.Height);
				ShadowViewport.MinDepth = 0.0f;
				ShadowViewport.MaxDepth = 1.0f;

				const FMatrix CascadeMatrix = LightComp->GetLightViewProj(
					CamView,
					CamProj,
					CascadeSplits[CascadeIndex].SplitNearRatio,
					CascadeSplits[CascadeIndex].SplitFarRatio,
					&VisibleBounds);

				FShadowConstants ShadowData = {};
				ShadowData.VirtualViewProj = CamView * CamProj;
				ShadowData.DirLightViewProj = CascadeMatrix;
				ShadowData.ScaleOffset = ShadowTile.ScaleOffset;

				RenderShadowDepth(
					Context,
					ShadowBuffer,
					ShadowShader,
					OpaqueCmds,
					ShadowDSV,
					ShadowViewport,
					ShadowKey,
					ShadowData);

				DirectionalShadowData.CascadeViewProj[CascadeIndex] = CascadeMatrix;
				DirectionalShadowData.CascadeScaleOffset[CascadeIndex] = ShadowTile.ScaleOffset;
				::SetCascadeSplitFar(
					DirectionalShadowData.CascadeSplitFar,
					CascadeIndex,
					CascadeSplits[CascadeIndex].FarDistance);
				DirectionalShadowData.DirectionalCascadeCount = CascadeIndex + 1;

				if (!bHasDirectionalShadow)
				{
					DirectionalShadowData.DirLightViewProj = ShadowData.DirLightViewProj;
					DirectionalShadowData.ScaleOffset = ShadowData.ScaleOffset;
					bHasDirectionalShadow = true;
				}
			}

			continue;
		}

		FShadowAtlasTile ShadowTile;
		if (!FShadowAtlasManager::Get().AllocateTile(ShadowTile))
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

		FShadowConstants ShadowData = {};
		ShadowData.VirtualViewProj = CamView * CamProj;

		ShadowData.DirLightViewProj = LightComp->GetLightViewProj(
			CamView,
			CamProj,
			&VisibleBounds);

		ShadowData.ScaleOffset = ShadowTile.ScaleOffset;

		RenderShadowDepth(
			Context,
			ShadowBuffer,
			ShadowShader,
			OpaqueCmds,
			ShadowDSV,
			ShadowViewport,
			ShadowKey,
			ShadowData);

		uint32 ShadowIndex = static_cast<uint32>(ShadowAtlasConstants.size());
		if (Request.LightIndex != InvalidShadowIndex && Request.LightIndex < LightShadowIndices.size())
		{
			LightShadowIndices[Request.LightIndex] = ShadowIndex;
		}

		float tile = static_cast<float>(ShadowTile.Width);

		FShadowAtlasConstants atlasConstants = {};
		atlasConstants.ShadowViewProjMatrix = ShadowData.DirLightViewProj;
		atlasConstants.ScaleOffset = FVector4(
			tile / atlasW,
			tile / atlasH,
			(ShadowTile.TileX * tile) / atlasW,
			(ShadowTile.TileY * tile) / atlasH);
		atlasConstants.ShadowBias = Request.ShadowBias;
		atlasConstants.ShadowStrength = 1.0f;
		atlasConstants.ShadowSoftness = Request.ShadowSharpen;
		atlasConstants.ShadowType = static_cast<uint32>(Request.Type);
		ShadowAtlasConstants.push_back(atlasConstants);
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

void FShadowPass::BuildPracticalCascadeSplit(float CamNear, float CamFar, float MaxShadowDistance, float Lambda, FCascadeSplit OutSplit[4])
{
	float Distance[CascadeCount + 1] = {};

	float ShadowNear = std::max(CamNear, 1.0f);
	float ShadowFar = std::min(CamFar, MaxShadowDistance);

	Distance[0] = ShadowNear;
	for (uint32 i = 1; i < CascadeCount; ++i)
	{
		const float P = static_cast<float>(i) / static_cast<float>(CascadeCount);
		float LogSplit = ShadowNear* std::pow(ShadowFar / ShadowNear, P);
		float LinearSplit = ShadowNear + (ShadowFar - ShadowNear) * P;

		Distance[i] = std::lerp(LogSplit, LinearSplit, Lambda);
	}

	Distance[CascadeCount] = ShadowFar;

	const float CameraRange = std::max(CamFar - CamNear, 0.001f);
	const float InvCameraRange = 1.0f / CameraRange;

	for (uint32 i = 0; i < CascadeCount; ++i)
	{
		OutSplit[i].NearDistance = Distance[i];
		OutSplit[i].FarDistance = Distance[i + 1];
		OutSplit[i].SplitNearRatio = std::clamp((Distance[i] - CamNear) * InvCameraRange, 0.0f, 1.0f);
		OutSplit[i].SplitFarRatio = std::clamp((Distance[i + 1] - CamNear) * InvCameraRange, 0.0f, 1.0f);
	}
}

bool FShadowPass::Release()
{
	return true;
}
