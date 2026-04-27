#include "RenderResources.h"
#include "Render/Device/D3DDevice.h"
#include "Materials/MaterialManager.h"
#include "Render/Types/ForwardLightData.h"
#include "Render/Types/FrameContext.h"
#include "Render/Scene/FScene.h"
#include "Engine/Runtime/Engine.h"
#include "Profiling/Timer.h"
#include "GameFramework/World.h"

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
	ShadowConstantBuffer.Create(InDevice, sizeof(FShadowCBData));
	ForwardLights.Create(InDevice, 32);

	RasterizerStateManager.Create(InDevice);
	DepthStencilStateManager.Create(InDevice);
	BlendStateManager.Create(InDevice);
	SamplerStateManager.Create(InDevice);

	FMaterialManager::Get().Initialize(InDevice);
}

void FShadowMapResources::EnsureCSM(ID3D11Device* Device, uint32 Resolution)
{
	if (CSMResolution == Resolution && CSMTexture) return;

	// 기존 CSM 리소스 해제
	if (CSMSRV) { CSMSRV->Release(); CSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (CSMSliceSRV[i]) { CSMSliceSRV[i]->Release(); CSMSliceSRV[i] = nullptr; }
		if (CSMDSV[i]) { CSMDSV[i]->Release(); CSMDSV[i] = nullptr; }
	}
	if (CSMTexture) { CSMTexture->Release(); CSMTexture = nullptr; }

	CSMResolution = Resolution;

	// Texture2DArray: ArraySize = MAX_SHADOW_CASCADES, R32_TYPELESS
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width  = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = MAX_SHADOW_CASCADES;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage  = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &CSMTexture);
	if (FAILED(hr)) return;

	// Per-cascade DSV (Texture2DArray slice)
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateDepthStencilView(CSMTexture, &DSVDesc, &CSMDSV[i]);
	}

	// SRV — 전체 array (셰이더용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = MAX_SHADOW_CASCADES;

	Device->CreateShaderResourceView(CSMTexture, &SRVDesc, &CSMSRV);

	// Per-cascade slice SRV (single slice — ImGui 디버그용)
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SliceSRVDesc = {};
		SliceSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SliceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		SliceSRVDesc.Texture2DArray.MipLevels = 1;
		SliceSRVDesc.Texture2DArray.MostDetailedMip = 0;
		SliceSRVDesc.Texture2DArray.FirstArraySlice = i;
		SliceSRVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateShaderResourceView(CSMTexture, &SliceSRVDesc, &CSMSliceSRV[i]);
	}
}

void FShadowMapResources::EnsureSpotAtlas(ID3D11Device* Device, uint32 Resolution, uint32 PageCount)
{
	if (PageCount == 0) return;

	// 리사이즈 또는 slice 수 변경 시에만 재생성
	if (SpotAtlasResolution == Resolution && SpotAtlasPageCount == PageCount && SpotAtlasTexture)
		return;

	// 기존 리소스 해제
	if (SpotAtlasSRV) { SpotAtlasSRV->Release(); SpotAtlasSRV = nullptr; }
	if (SpotAtlasSliceSRVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
		{
			if (SpotAtlasSliceSRVs[i]) SpotAtlasSliceSRVs[i]->Release();
		}
		delete[] SpotAtlasSliceSRVs;
		SpotAtlasSliceSRVs = nullptr;
	}
	if (SpotAtlasDSVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
		{
			if (SpotAtlasDSVs[i]) SpotAtlasDSVs[i]->Release();
		}
		delete[] SpotAtlasDSVs;
		SpotAtlasDSVs = nullptr;
	}
	if (SpotAtlasTexture) { SpotAtlasTexture->Release(); SpotAtlasTexture = nullptr; }
	if (SpotShadowDataSRV)    { SpotShadowDataSRV->Release();    SpotShadowDataSRV = nullptr; }
	if (SpotShadowDataBuffer) { SpotShadowDataBuffer->Release(); SpotShadowDataBuffer = nullptr; }
	SpotShadowDataCapacity = 0;

	SpotAtlasResolution = Resolution;
	SpotAtlasPageCount  = 1;

	// Texture2DArray: 1 slice = 1 spot light
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width  = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage  = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &SpotAtlasTexture);
	if (FAILED(hr)) return;

	// Per-slice DSV
	SpotAtlasDSVs = new ID3D11DepthStencilView*[1]();
	for (uint32 i = 0; i < 1; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateDepthStencilView(SpotAtlasTexture, &DSVDesc, &SpotAtlasDSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = 1;

	Device->CreateShaderResourceView(SpotAtlasTexture, &SRVDesc, &SpotAtlasSRV);

	// Per-slice SRV (ImGui 디버그용)
	SpotAtlasSliceSRVs = new ID3D11ShaderResourceView*[1]();
	for (uint32 i = 0; i < 1; ++i)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SliceSRVDesc = {};
		SliceSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
		SliceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		SliceSRVDesc.Texture2DArray.MipLevels = 1;
		SliceSRVDesc.Texture2DArray.MostDetailedMip = 0;
		SliceSRVDesc.Texture2DArray.FirstArraySlice = i;
		SliceSRVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateShaderResourceView(SpotAtlasTexture, &SliceSRVDesc, &SpotAtlasSliceSRVs[i]);
	}

	// StructuredBuffer<FSpotShadowDataGPU> — per-light 행렬 데이터
	SpotShadowDataCapacity = PageCount;

	D3D11_BUFFER_DESC BufDesc = {};
	BufDesc.ByteWidth = sizeof(FSpotShadowDataGPU) * PageCount;
	BufDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufDesc.StructureByteStride = sizeof(FSpotShadowDataGPU);

	Device->CreateBuffer(&BufDesc, nullptr, &SpotShadowDataBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC SBSRVDesc = {};
	SBSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SBSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SBSRVDesc.Buffer.NumElements = PageCount;

	Device->CreateShaderResourceView(SpotShadowDataBuffer, &SBSRVDesc, &SpotShadowDataSRV);
}

void FShadowMapResources::EnsurePointCube(ID3D11Device* Device, uint32 Resolution, uint32 CubeCount)
{
	if (PointCubeResolution == Resolution && PointCubeCount == CubeCount && PointCubeTexture)
		return;

	// 기존 리소스 해제
	if (PointCubeSRV)
	{
		PointCubeSRV->Release();
		PointCubeSRV = nullptr;
	}

	if (PointCubeDSVs)
	{
		for (uint32 i = 0; i < PointCubeCount; ++i)
		{
			for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
			{
				if (PointCubeDSVs[i * 6 + FaceIndex])
				{
					PointCubeDSVs[i * 6 + FaceIndex]->Release();
				}
			}
		}
		delete[] PointCubeDSVs;
		PointCubeDSVs = nullptr;
	}

	if (PointCubeTexture)
	{
		PointCubeTexture->Release();
		PointCubeTexture = nullptr;
	}
	if (PointShadowDataSRV)
	{
		PointShadowDataSRV->Release();
		PointShadowDataSRV = nullptr;
	}
	if (PointShadowDataBuffer)
	{
		PointShadowDataBuffer->Release();
		PointShadowDataBuffer = nullptr;
	}

	PointShadowDataCapacity = 0;
	PointCubeCount = 0;

	if (CubeCount == 0)
	{
		return;
	}

	PointCubeResolution = Resolution;

	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	// TODO: ArraySize 0으로 만들어보기
	TexDesc.ArraySize = CubeCount * 6;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	TexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &PointCubeTexture);
	if (FAILED(hr)) return;

	PointCubeCount = CubeCount;
	PointCubeDSVs = new ID3D11DepthStencilView *[CubeCount * 6]();
	for (uint32 CubeIndex = 0; CubeIndex < CubeCount; ++CubeIndex)
	{
		for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			uint32 CubeFaceIndex = CubeIndex * 6 + FaceIndex;

			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
			DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
			DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			DSVDesc.Texture2DArray.MipSlice = 0;
			DSVDesc.Texture2DArray.FirstArraySlice = CubeFaceIndex;
			DSVDesc.Texture2DArray.ArraySize = 1;

			Device->CreateDepthStencilView(PointCubeTexture, &DSVDesc, &PointCubeDSVs[CubeFaceIndex]);
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	SRVDesc.TextureCubeArray.MostDetailedMip = 0;
	SRVDesc.TextureCubeArray.MipLevels = 1;
	SRVDesc.TextureCubeArray.First2DArrayFace = 0;
	SRVDesc.TextureCubeArray.NumCubes = CubeCount;

	Device->CreateShaderResourceView(PointCubeTexture, &SRVDesc, &PointCubeSRV);

	PointShadowDataCapacity = CubeCount;

	// StructuredBuffer<FPointShadowDataGPU>
	D3D11_BUFFER_DESC BufferDesc = {};
	BufferDesc.ByteWidth = sizeof(FPointShadowDataGPU) * CubeCount;
	BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(FPointShadowDataGPU);

	Device->CreateBuffer(&BufferDesc, nullptr, &PointShadowDataBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC BufferSRVDesc = {};
	BufferSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	BufferSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	BufferSRVDesc.Buffer.NumElements = CubeCount;

	Device->CreateShaderResourceView(PointShadowDataBuffer, &BufferSRVDesc, &PointShadowDataSRV);
}

// ============================================================
// VSM Ensure 함수들 — R32G32_FLOAT moment + D32_FLOAT depth
// ============================================================

void FShadowMapResources::EnsureCSM_VSM(ID3D11Device* Device, uint32 Resolution)
{
	if (CSMResolution == Resolution && CSMVSMTexture) return;

	// 기존 VSM CSM 리소스 해제
	if (CSMVSMSRV) { CSMVSMSRV->Release(); CSMVSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (CSMVSMSliceSRV[i]) { CSMVSMSliceSRV[i]->Release(); CSMVSMSliceSRV[i] = nullptr; }
		if (CSMVSMRTV[i]) { CSMVSMRTV[i]->Release(); CSMVSMRTV[i] = nullptr; }
		if (CSMVSMDSV[i]) { CSMVSMDSV[i]->Release(); CSMVSMDSV[i] = nullptr; }
	}
	if (CSMVSMTexture) { CSMVSMTexture->Release(); CSMVSMTexture = nullptr; }
	if (CSMVSMDepthTexture) { CSMVSMDepthTexture->Release(); CSMVSMDepthTexture = nullptr; }

	// Moment Texture: R32G32_FLOAT, Texture2DArray
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width  = Resolution;
	MomentDesc.Height = Resolution;
	MomentDesc.MipLevels = 1;
	MomentDesc.ArraySize = MAX_SHADOW_CASCADES;
	MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage  = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &CSMVSMTexture))) return;

	// Depth Texture: D32_FLOAT (DSV 전용, SRV 불필요)
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width  = Resolution;
	DepthDesc.Height = Resolution;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = MAX_SHADOW_CASCADES;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage  = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &CSMVSMDepthTexture)))
	{
		CSMVSMTexture->Release(); CSMVSMTexture = nullptr;
		return;
	}

	// Per-cascade RTV + DSV
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateRenderTargetView(CSMVSMTexture, &RTVDesc, &CSMVSMRTV[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateDepthStencilView(CSMVSMDepthTexture, &DSVDesc, &CSMVSMDSV[i]);
	}

	// SRV — 전체 array (셰이더에서 moment.rg로 읽음)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = MAX_SHADOW_CASCADES;
	Device->CreateShaderResourceView(CSMVSMTexture, &SRVDesc, &CSMVSMSRV);

	// Per-cascade slice SRV (ImGui 디버그용)
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SliceSRVDesc = {};
		SliceSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		SliceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		SliceSRVDesc.Texture2DArray.MipLevels = 1;
		SliceSRVDesc.Texture2DArray.MostDetailedMip = 0;
		SliceSRVDesc.Texture2DArray.FirstArraySlice = i;
		SliceSRVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateShaderResourceView(CSMVSMTexture, &SliceSRVDesc, &CSMVSMSliceSRV[i]);
	}
}

void FShadowMapResources::EnsureSpotAtlas_VSM(ID3D11Device* Device, uint32 Resolution, uint32 PageCount)
{
	if (PageCount == 0) return;
	if (SpotAtlasResolution == Resolution && SpotAtlasPageCount == PageCount && SpotVSMTexture)
		return;

	// 기존 Spot VSM 리소스 해제
	if (SpotVSMSRV) { SpotVSMSRV->Release(); SpotVSMSRV = nullptr; }
	if (SpotVSMRTVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
			if (SpotVSMRTVs[i]) SpotVSMRTVs[i]->Release();
		delete[] SpotVSMRTVs;
		SpotVSMRTVs = nullptr;
	}
	if (SpotVSMDSVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
			if (SpotVSMDSVs[i]) SpotVSMDSVs[i]->Release();
		delete[] SpotVSMDSVs;
		SpotVSMDSVs = nullptr;
	}
	if (SpotVSMTexture) { SpotVSMTexture->Release(); SpotVSMTexture = nullptr; }
	if (SpotVSMDepthTexture) { SpotVSMDepthTexture->Release(); SpotVSMDepthTexture = nullptr; }

	// Moment Texture
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width  = Resolution;
	MomentDesc.Height = Resolution;
	MomentDesc.MipLevels = 1;
	MomentDesc.ArraySize = PageCount;
	MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage  = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &SpotVSMTexture))) return;

	// Depth Texture
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width  = Resolution;
	DepthDesc.Height = Resolution;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = PageCount;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage  = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &SpotVSMDepthTexture)))
	{
		SpotVSMTexture->Release(); SpotVSMTexture = nullptr;
		return;
	}

	// Per-slice RTV + DSV
	SpotVSMRTVs = new ID3D11RenderTargetView*[PageCount]();
	SpotVSMDSVs = new ID3D11DepthStencilView*[PageCount]();
	for (uint32 i = 0; i < PageCount; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateRenderTargetView(SpotVSMTexture, &RTVDesc, &SpotVSMRTVs[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateDepthStencilView(SpotVSMDepthTexture, &DSVDesc, &SpotVSMDSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = PageCount;
	Device->CreateShaderResourceView(SpotVSMTexture, &SRVDesc, &SpotVSMSRV);
}

void FShadowMapResources::EnsurePointCube_VSM(ID3D11Device* Device, uint32 Resolution, uint32 CubeCount)
{
	if (PointCubeResolution == Resolution && PointCubeCount == CubeCount && PointVSMTexture)
		return;

	// 기존 Point VSM 리소스 해제
	if (PointVSMSRV) { PointVSMSRV->Release(); PointVSMSRV = nullptr; }
	if (PointVSMRTVs)
	{
		for (uint32 i = 0; i < PointCubeCount * 6; ++i)
			if (PointVSMRTVs[i]) PointVSMRTVs[i]->Release();
		delete[] PointVSMRTVs;
		PointVSMRTVs = nullptr;
	}
	if (PointVSMDSVs)
	{
		for (uint32 i = 0; i < PointCubeCount * 6; ++i)
			if (PointVSMDSVs[i]) PointVSMDSVs[i]->Release();
		delete[] PointVSMDSVs;
		PointVSMDSVs = nullptr;
	}
	if (PointVSMTexture) { PointVSMTexture->Release(); PointVSMTexture = nullptr; }
	if (PointVSMDepthTexture) { PointVSMDepthTexture->Release(); PointVSMDepthTexture = nullptr; }

	if (CubeCount == 0) return;

	// Moment Texture: R32G32_FLOAT, TextureCubeArray
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width  = Resolution;
	MomentDesc.Height = Resolution;
	MomentDesc.MipLevels = 1;
	MomentDesc.ArraySize = CubeCount * 6;
	MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage  = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	MomentDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &PointVSMTexture))) return;

	// Depth Texture: D32_FLOAT, TextureCubeArray
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width  = Resolution;
	DepthDesc.Height = Resolution;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = CubeCount * 6;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage  = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &PointVSMDepthTexture)))
	{
		PointVSMTexture->Release(); PointVSMTexture = nullptr;
		return;
	}

	// Per-face RTV + DSV
	const uint32 TotalFaces = CubeCount * 6;
	PointVSMRTVs = new ID3D11RenderTargetView*[TotalFaces]();
	PointVSMDSVs = new ID3D11DepthStencilView*[TotalFaces]();
	for (uint32 i = 0; i < TotalFaces; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateRenderTargetView(PointVSMTexture, &RTVDesc, &PointVSMRTVs[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateDepthStencilView(PointVSMDepthTexture, &DSVDesc, &PointVSMDSVs[i]);
	}

	// SRV — TextureCubeArray
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	SRVDesc.TextureCubeArray.MostDetailedMip = 0;
	SRVDesc.TextureCubeArray.MipLevels = 1;
	SRVDesc.TextureCubeArray.First2DArrayFace = 0;
	SRVDesc.TextureCubeArray.NumCubes = CubeCount;
	Device->CreateShaderResourceView(PointVSMTexture, &SRVDesc, &PointVSMSRV);
}

void FShadowMapResources::ReleaseVSM()
{
	// CSM VSM
	if (CSMVSMSRV) { CSMVSMSRV->Release(); CSMVSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (CSMVSMSliceSRV[i]) { CSMVSMSliceSRV[i]->Release(); CSMVSMSliceSRV[i] = nullptr; }
		if (CSMVSMRTV[i]) { CSMVSMRTV[i]->Release(); CSMVSMRTV[i] = nullptr; }
		if (CSMVSMDSV[i]) { CSMVSMDSV[i]->Release(); CSMVSMDSV[i] = nullptr; }
	}
	if (CSMVSMTexture) { CSMVSMTexture->Release(); CSMVSMTexture = nullptr; }
	if (CSMVSMDepthTexture) { CSMVSMDepthTexture->Release(); CSMVSMDepthTexture = nullptr; }

	// Spot VSM
	if (SpotVSMSRV) { SpotVSMSRV->Release(); SpotVSMSRV = nullptr; }
	if (SpotVSMRTVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
			if (SpotVSMRTVs[i]) SpotVSMRTVs[i]->Release();
		delete[] SpotVSMRTVs;
		SpotVSMRTVs = nullptr;
	}
	if (SpotVSMDSVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
			if (SpotVSMDSVs[i]) SpotVSMDSVs[i]->Release();
		delete[] SpotVSMDSVs;
		SpotVSMDSVs = nullptr;
	}
	if (SpotVSMTexture) { SpotVSMTexture->Release(); SpotVSMTexture = nullptr; }
	if (SpotVSMDepthTexture) { SpotVSMDepthTexture->Release(); SpotVSMDepthTexture = nullptr; }

	// Point VSM
	if (PointVSMSRV) { PointVSMSRV->Release(); PointVSMSRV = nullptr; }
	if (PointVSMRTVs)
	{
		for (uint32 i = 0; i < PointCubeCount * 6; ++i)
			if (PointVSMRTVs[i]) PointVSMRTVs[i]->Release();
		delete[] PointVSMRTVs;
		PointVSMRTVs = nullptr;
	}
	if (PointVSMDSVs)
	{
		for (uint32 i = 0; i < PointCubeCount * 6; ++i)
			if (PointVSMDSVs[i]) PointVSMDSVs[i]->Release();
		delete[] PointVSMDSVs;
		PointVSMDSVs = nullptr;
	}
	if (PointVSMTexture) { PointVSMTexture->Release(); PointVSMTexture = nullptr; }
	if (PointVSMDepthTexture) { PointVSMDepthTexture->Release(); PointVSMDepthTexture = nullptr; }
}

void FShadowMapResources::Release()
{
	ReleaseVSM();

	// CSM
	if (CSMSRV) { CSMSRV->Release(); CSMSRV = nullptr; }
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		if (CSMSliceSRV[i]) { CSMSliceSRV[i]->Release(); CSMSliceSRV[i] = nullptr; }
		if (CSMDSV[i]) { CSMDSV[i]->Release(); CSMDSV[i] = nullptr; }
	}
	if (CSMTexture) { CSMTexture->Release(); CSMTexture = nullptr; }

	// Spot Atlas
	if (SpotAtlasSRV) { SpotAtlasSRV->Release(); SpotAtlasSRV = nullptr; }
	if (SpotAtlasSliceSRVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
		{
			if (SpotAtlasSliceSRVs[i]) SpotAtlasSliceSRVs[i]->Release();
		}
		delete[] SpotAtlasSliceSRVs;
		SpotAtlasSliceSRVs = nullptr;
	}
	if (SpotAtlasDSVs)
	{
		for (uint32 i = 0; i < SpotAtlasPageCount; ++i)
		{
			if (SpotAtlasDSVs[i]) SpotAtlasDSVs[i]->Release();
		}
		delete[] SpotAtlasDSVs;
		SpotAtlasDSVs = nullptr;
	}
	if (SpotAtlasTexture) { SpotAtlasTexture->Release(); SpotAtlasTexture = nullptr; }
	SpotAtlasPageCount = 0;

	// Point Cube
	if (PointCubeSRV) { PointCubeSRV->Release(); PointCubeSRV = nullptr; }
	if (PointCubeDSVs)
	{
		for (uint32 i = 0; i < PointCubeCount * 6; ++i)
		{
			if (PointCubeDSVs[i]) PointCubeDSVs[i]->Release();
		}
		delete[] PointCubeDSVs;
		PointCubeDSVs = nullptr;
	}
	if (PointCubeTexture) { PointCubeTexture->Release(); PointCubeTexture = nullptr; }
	PointCubeCount = 0;

	// StructuredBuffers
	if (SpotShadowDataSRV)    { SpotShadowDataSRV->Release();    SpotShadowDataSRV = nullptr; }
	if (SpotShadowDataBuffer) { SpotShadowDataBuffer->Release(); SpotShadowDataBuffer = nullptr; }
	SpotShadowDataCapacity = 0;

	if (PointShadowDataSRV)    { PointShadowDataSRV->Release();    PointShadowDataSRV = nullptr; }
	if (PointShadowDataBuffer) { PointShadowDataBuffer->Release(); PointShadowDataBuffer = nullptr; }
	PointShadowDataCapacity = 0;
}

void FSystemResources::Release()
{
	ShadowResources.Release();

	SamplerStateManager.Release();
	BlendStateManager.Release();
	DepthStencilStateManager.Release();
	RasterizerStateManager.Release();

	FrameBuffer.Release();
	LightingConstantBuffer.Release();
	ShadowConstantBuffer.Release();
	ForwardLights.Release();
	TileCullingResource.Release();
}

void FSystemResources::UpdateFrameBuffer(FD3DDevice& Device, const FFrameContext& Frame)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	FFrameConstants frameConstantData = {};
	frameConstantData.View = Frame.View;
	frameConstantData.Projection = Frame.Proj;
	frameConstantData.InvProj = Frame.Proj.GetInverse();
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
	Ctx->CSSetConstantBuffers(ECBSlot::Frame, 1, &b0);
}

void FSystemResources::UpdateLightBuffer(FD3DDevice& Device, const FScene& Scene, const FFrameContext& Frame)
{
	ID3D11Device* Dev = Device.GetDevice();
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();

	FLightingCBData GlobalLightingData = {};
	const FSceneEnvironment& Env = Scene.GetEnvironment();
	if (Env.HasGlobalAmbientLight())
	{
		FGlobalAmbientLightParams DirLightParams = Env.GetGlobalAmbientLightParams();
		GlobalLightingData.Ambient.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Ambient.Color = DirLightParams.LightColor;
	}
	else
	{
		// 폴백: 씬에 AmbientLight 없으면 최소 ambient 보장 (검정 방지)
		GlobalLightingData.Ambient.Intensity = 0.15f;
		GlobalLightingData.Ambient.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	if (Env.HasGlobalDirectionalLight())
	{
		FGlobalDirectionalLightParams DirLightParams = Env.GetGlobalDirectionalLightParams();
		GlobalLightingData.Directional.Intensity = DirLightParams.Intensity;
		GlobalLightingData.Directional.Color = DirLightParams.LightColor;
		GlobalLightingData.Directional.Direction = DirLightParams.Direction;
	}

	const uint32 NumPointLights = Env.GetNumPointLights();
	const uint32 NumSpotLights  = Env.GetNumSpotLights();
	GlobalLightingData.NumActivePointLights = NumPointLights;
	GlobalLightingData.NumActiveSpotLights  = NumSpotLights;

	TArray<FLightInfo> Infos;
	Infos.reserve(NumPointLights + NumSpotLights);

	// Point lights — ShadowMapIndex 순차 할당 (shadow-casting만)
	uint32 PointShadowIdx = 0;
	for (uint32 i = 0; i < NumPointLights; ++i)
	{
		FLightInfo Info = Env.GetPointLight(i).ToLightInfo();
		if (Info.bCastShadow)
			Info.ShadowMapIndex = PointShadowIdx++;
		Infos.emplace_back(Info);
	}

	// Spot lights — ShadowMapIndex 순차 할당 (RenderSpotShadows와 동일 순서)
	uint32 SpotShadowIdx = 0;
	for (uint32 i = 0; i < NumSpotLights; ++i)
	{
		FLightInfo Info = Env.GetSpotLight(i).ToLightInfo();
		if (Info.bCastShadow)
			Info.ShadowMapIndex = SpotShadowIdx++;
		Infos.emplace_back(Info);
	}

	LastNumLights = static_cast<uint32>(Infos.size());

	GlobalLightingData.LightCullingMode = static_cast<uint32>(Frame.RenderOptions.LightCullingMode);
	GlobalLightingData.VisualizeLightCulling = Frame.RenderOptions.ViewMode == EViewMode::LightCulling ? 1u : 0u;
	GlobalLightingData.HeatMapMax = Frame.RenderOptions.HeatMapMax;

	ClusteredLightCuller.UpdateFrameState(Frame);
	GlobalLightingData.ClusterCullingState = ClusteredLightCuller.GetCullingState();

	// 이전 프레임 타일 컬링 결과에서 타일 수 읽기 (1-frame latent)
	GlobalLightingData.NumTilesX = TileCullingResource.TileCountX;
	GlobalLightingData.NumTilesY = TileCullingResource.TileCountY;

	LightingConstantBuffer.Update(Ctx, &GlobalLightingData, sizeof(FLightingCBData));
	ID3D11Buffer* b4 = LightingConstantBuffer.GetBuffer();
	Ctx->VSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);
	Ctx->PSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);
	Ctx->CSSetConstantBuffers(ECBSlot::Lighting, 1, &b4);

	ForwardLights.Update(Dev, Ctx, Infos);
	Ctx->VSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);

	if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Tile)
	{
		BindTileCullingBuffers(Device);
	}
	else
	{
		UnbindTileCullingBuffers(Device);
	}
}

void FSystemResources::BindTileCullingBuffers(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	Ctx->VSSetShaderResources(ELightTexSlot::TileLightIndices, 1, &TileCullingResource.IndicesSRV);
	Ctx->VSSetShaderResources(ELightTexSlot::TileLightGrid,    1, &TileCullingResource.GridSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::TileLightIndices, 1, &TileCullingResource.IndicesSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::TileLightGrid,    1, &TileCullingResource.GridSRV);
	Ctx->VSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
	Ctx->PSSetShaderResources(ELightTexSlot::AllLights, 1, &ForwardLights.LightBufferSRV);
}

void FSystemResources::UnbindTileCullingBuffers(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* NullSRVs[2] = {};
	Ctx->VSSetShaderResources(ELightTexSlot::TileLightIndices, 2, NullSRVs);
	Ctx->PSSetShaderResources(ELightTexSlot::TileLightIndices, 2, NullSRVs);
	Ctx->CSSetShaderResources(ELightTexSlot::TileLightIndices, 2, NullSRVs);
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
	ID3D11ShaderResourceView* nullSRVs[5] = {};

	// t16~t20: Scene Depth/Color/Normal/Stencil/Heatmap
	Ctx->PSSetShaderResources(ESystemTexSlot::SceneDepth, 5, nullSRVs);

	// t21~t25: Shadow (CSM/SpotAtlas/PointCube/SpotData/PointData)
	Ctx->PSSetShaderResources(ESystemTexSlot::ShadowMapCSM, 5, nullSRVs);
}

// ============================================================
// Light Culling Facade
// ============================================================

void FSystemResources::DispatchTileCulling(ID3D11DeviceContext* DC, const FFrameContext& Frame)
{
	TileBasedCulling.Dispatch(
		DC, Frame,
		FrameBuffer.GetBuffer(),
		TileCullingResource,
		ForwardLights.LightBufferSRV,
		LastNumLights,
		static_cast<uint32>(Frame.ViewportWidth),
		static_cast<uint32>(Frame.ViewportHeight));
}

void FSystemResources::DispatchClusterCulling(FD3DDevice& Device)
{
	if (!ClusteredLightCuller.IsInitialized()) return;

	UnbindTileCullingBuffers(Device);
	UnbindClusterCullingResources(Device);

	ClusteredLightCuller.DispatchLightCullingCS(ForwardLights.LightBufferSRV);

	BindClusterCullingResources(Device);
}

void FSystemResources::BindClusterCullingResources(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* LightIndexList = ClusteredLightCuller.GetLightIndexListSRV();
	ID3D11ShaderResourceView* LightGridList  = ClusteredLightCuller.GetLightGridSRV();
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 1, &LightIndexList);
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightGrid, 1, &LightGridList);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 1, &LightIndexList);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightGrid, 1, &LightGridList);
}

void FSystemResources::UnbindClusterCullingResources(FD3DDevice& Device)
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* NullSRVs[2] = {};
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
	Ctx->CSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
}

void FSystemResources::SubmitCullingDebugLines(ID3D11DeviceContext* DC, UWorld* World)
{
	TileBasedCulling.SubmitVisualizationDebugLines(DC, World);
}
