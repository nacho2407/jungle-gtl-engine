#include "RenderResources.h"
#include "Render/Device/D3DDevice.h"
#include "Materials/MaterialManager.h"
#include "Render/Pipeline/ForwardLightData.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Proxy/FScene.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"

void FTileCullingResource::Create(ID3D11Device* Dev, uint32 InTileCountX, uint32 InTileCountY)
{
	Release();
	TileCountX = InTileCountX;
	TileCountY = InTileCountY;
	const uint32 NumTiles = TileCountX * TileCountY;

	auto MakeStructured = [&](
		uint32 ElemCount, uint32 Stride,
		ID3D11Buffer** OutBuf,
		ID3D11UnorderedAccessView** OutUAV,
		ID3D11ShaderResourceView** OutSRV)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth          = ElemCount * Stride;
		bd.Usage              = D3D11_USAGE_DEFAULT;
		bd.BindFlags          = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags          = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = Stride;
		Dev->CreateBuffer(&bd, nullptr, OutBuf);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
		uavd.Format                  = DXGI_FORMAT_UNKNOWN;
		uavd.ViewDimension           = D3D11_UAV_DIMENSION_BUFFER;
		uavd.Buffer.NumElements      = ElemCount;
		Dev->CreateUnorderedAccessView(*OutBuf, &uavd, OutUAV);

		if (OutSRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
			srvd.Format              = DXGI_FORMAT_UNKNOWN;
			srvd.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
			srvd.Buffer.NumElements  = ElemCount;
			Dev->CreateShaderResourceView(*OutBuf, &srvd, OutSRV);
		}
	};

	// t9 / u0: TileLightIndices — uint per slot per tile
	MakeStructured(ETileCulling::MaxLightsPerTile * NumTiles, sizeof(uint32),
		&IndicesBuffer, &IndicesUAV, &IndicesSRV);

	// t10 / u1: TileLightGrid — uint2 (offset, count) per tile
	MakeStructured(NumTiles, sizeof(uint32) * 2,
		&GridBuffer, &GridUAV, &GridSRV);

	// u2: GlobalLightCounter — single atomic uint
	MakeStructured(1, sizeof(uint32),
		&CounterBuffer, &CounterUAV, nullptr);
}

void FTileCullingResource::Release()
{
	if (IndicesSRV)  { IndicesSRV->Release();  IndicesSRV  = nullptr; }
	if (GridSRV)     { GridSRV->Release();     GridSRV     = nullptr; }
	if (IndicesUAV)  { IndicesUAV->Release();  IndicesUAV  = nullptr; }
	if (GridUAV)     { GridUAV->Release();     GridUAV     = nullptr; }
	if (CounterUAV)  { CounterUAV->Release();  CounterUAV  = nullptr; }
	if (IndicesBuffer)  { IndicesBuffer->Release();  IndicesBuffer  = nullptr; }
	if (GridBuffer)     { GridBuffer->Release();     GridBuffer     = nullptr; }
	if (CounterBuffer)  { CounterBuffer->Release();  CounterBuffer  = nullptr; }
	TileCountX = TileCountY = 0;
}

void FSystemResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	LightingConstantBuffer.Create(InDevice, sizeof(FLightingCBData));
	ForwardLights.Create(InDevice, 32);

	RasterizerStateManager.Create(InDevice);
	DepthStencilStateManager.Create(InDevice);
	BlendStateManager.Create(InDevice);
	SamplerStateManager.Create(InDevice);

	FMaterialManager::Get().Initialize(InDevice);
}

void FSystemResources::Release()
{
	SamplerStateManager.Release();
	BlendStateManager.Release();
	DepthStencilStateManager.Release();
	RasterizerStateManager.Release();

	FrameBuffer.Release();
	LightingConstantBuffer.Release();
	ForwardLights.Release();
	TileCullingResource.Release();
}

void FSystemResources::UpdateFrameBuffer(FD3DDevice& Device, const FFrameContext& Frame)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	FFrameConstants frameConstantData = {};
	frameConstantData.View = Frame.View;
	frameConstantData.Projection = Frame.Proj;
	frameConstantData.InvViewProj = (Frame.View * Frame.Proj).GetInverse();
	frameConstantData.bIsWireframe = (Frame.RenderOptions.ViewMode == EViewMode::Wireframe);
	frameConstantData.WireframeColor = Frame.WireframeColor;
	frameConstantData.CameraWorldPos = Frame.CameraPosition;

	if (GEngine && GEngine->GetTimer())
	{
		frameConstantData.Time = static_cast<float>(GEngine->GetTimer()->GetTotalTime());
	}

	FrameBuffer.Update(Ctx, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = FrameBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
	Ctx->PSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}

void FSystemResources::UpdateLightBuffer(FD3DDevice& Device, const FScene& Scene, const FFrameContext& Frame)
{
	ID3D11Device* Dev = Device.GetDevice();
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	FLightingCBData GlobalLightingData = {};
	if (Scene.HasGlobalAmbientLight())
	{
		FGlobalAmbientLightParams DirLightParams = Scene.GetGlobalAmbientLightParams();
		GlobalLightingData.Ambient.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Ambient.Color = DirLightParams.LightColor;
	}
	else
	{
		// 폴백: 씬에 AmbientLight 없으면 최소 ambient 보장 (검정 방지)
		GlobalLightingData.Ambient.Intensity = 0.15f;
		GlobalLightingData.Ambient.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	if (Scene.HasGlobalDirectionalLight())
	{
		FGlobalDirectionalLightParams DirLightParams = Scene.GetGlobalDirectionalLightParams();
		GlobalLightingData.Directional.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Directional.Color = DirLightParams.LightColor;
		GlobalLightingData.Directional.Direction = DirLightParams.Direction;
	}

	const TArray<FPointLightParams>& PointLightParams = Scene.GetPointLights();
	if (!PointLightParams.empty())
	{
		GlobalLightingData.NumActivePointLights = static_cast<uint32>(PointLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActivePointLights = 0;
	}

	const TArray<FSpotLightParams>& SpotLightParams = Scene.GetSpotLights();
	if (!SpotLightParams.empty())
	{
		GlobalLightingData.NumActiveSpotLights = static_cast<uint32>(SpotLightParams.size());
	}
	else
	{
		GlobalLightingData.NumActiveSpotLights = 0;
	}

	TArray<FLightInfo> Infos;
	for (const FPointLightParams& PointLigth : PointLightParams)
	{
		Infos.emplace_back(PointLigth.ToLightInfo());
	}
	for (const FSpotLightParams& SpotLight : SpotLightParams)
	{
		Infos.emplace_back(SpotLight.ToLightInfo());
	}

	LastNumLights = static_cast<uint32>(Infos.size());

	GlobalLightingData.ViewLightCulling = Frame.RenderOptions.ShowFlags.bViewLightCulling;
	GlobalLightingData.HeatMapMax = Frame.RenderOptions.HeatMapMax;

	// 이전 프레임 타일 컬링 결과에서 타일 수 읽기 (1-frame latent)
	GlobalLightingData.NumTilesX = TileCullingResource.TileCountX;
	GlobalLightingData.NumTilesY = TileCullingResource.TileCountY;

	LightingConstantBuffer.Update(Ctx, &GlobalLightingData, sizeof(FLightingCBData));
	ID3D11Buffer* b4 = LightingConstantBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);
	Ctx->PSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);

	ForwardLights.Update(Dev, Ctx, Infos);
	Ctx->VSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);

	// 이전 프레임 타일 컬링 결과 바인딩 (t9, t10)
	BindTileCullingBuffers(Device);
}

void FSystemResources::BindTileCullingBuffers(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	Ctx->PSSetShaderResources(ELightTexSlot::TileLightIndices, 1, &TileCullingResource.IndicesSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::TileLightGrid,    1, &TileCullingResource.GridSRV);
}

void FSystemResources::BindSystemSamplers(FD3DDevice& Device)
{
	SamplerStateManager.BindSystemSamplers(Device.GetDeviceContext());
}

void FSystemResources::SetDepthStencilState(FD3DDevice& Device, EDepthStencilState InState)
{
	DepthStencilStateManager.Set(Device.GetDeviceContext(), InState);
}

void FSystemResources::SetBlendState(FD3DDevice& Device, EBlendState InState)
{
	BlendStateManager.Set(Device.GetDeviceContext(), InState);
}

void FSystemResources::SetRasterizerState(FD3DDevice& Device, ERasterizerState InState)
{
	RasterizerStateManager.Set(Device.GetDeviceContext(), InState);
}

void FSystemResources::ResetRenderStateCache()
{
	RasterizerStateManager.ResetCache();
	DepthStencilStateManager.ResetCache();
	BlendStateManager.ResetCache();
}

void FSystemResources::UnbindSystemTextures(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* nullSRV = nullptr;
	Ctx->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &nullSRV);
	Ctx->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &nullSRV);
}
