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

// ============================================================
// Helper: COM view 배열 일괄 해제
// ============================================================

template<typename T>
static void ReleaseViewArray(TArray<T*>& Views)
{
	for (auto& V : Views)
	{
		if (V) { V->Release(); V = nullptr; }
	}
	Views.clear();
}

template<typename T>
static void ReleaseCOM(T*& Ptr)
{
	if (Ptr) { Ptr->Release(); Ptr = nullptr; }
}

// ============================================================
// FCSMResources
// ============================================================

void FShadowMapResources::FCSMResources::Release()
{
	// VSM
	ReleaseCOM(VSMSRV);
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		ReleaseCOM(VSMRTV[i]);
		ReleaseCOM(VSMDSV[i]);
	}
	ReleaseCOM(VSMTexture);
	ReleaseCOM(VSMDepthTexture);

	// Normal
	ReleaseCOM(SRV);
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		ReleaseCOM(DSV[i]);
	}
	ReleaseCOM(Texture);
	Resolution = 0;
}

// ============================================================
// FSpotResources
// ============================================================

void FShadowMapResources::FSpotResources::Release()
{
	// VSM
	ReleaseCOM(VSMSRV);
	ReleaseViewArray(VSMRTVs);
	ReleaseViewArray(VSMDSVs);
	ReleaseCOM(VSMTexture);
	ReleaseCOM(VSMDepthTexture);

	// Normal
	ReleaseCOM(SRV);
	ReleaseViewArray(DSVs);
	ReleaseCOM(Texture);
	PageCount = 0;
	Resolution = 0;

	// Data buffer
	ReleaseCOM(DataSRV);
	ReleaseCOM(DataBuffer);
	DataCapacity = 0;
}

// ============================================================
// FPointResources
// ============================================================

void FShadowMapResources::FPointResources::Release()
{
	// VSM
	ReleaseCOM(VSMSRV);
	ReleaseViewArray(VSMRTVs);
	ReleaseViewArray(VSMDSVs);
	ReleaseCOM(VSMTexture);
	ReleaseCOM(VSMDepthTexture);

	// Normal
	ReleaseCOM(SRV);
	ReleaseViewArray(DSVs);
	ReleaseCOM(Texture);
	PageCount = 0;
	Resolution = 0;

	// Data buffer
	ReleaseCOM(DataSRV);
	ReleaseCOM(DataBuffer);
	DataCapacity = 0;
}

// ============================================================
// FShadowMapResources — Ensure (Normal)
// ============================================================

void FShadowMapResources::EnsureCSM(ID3D11Device* Device, uint32 InResolution)
{
	if (CSM.Resolution == InResolution && CSM.Texture) return;

	// 기존 리소스 해제
	ReleaseCOM(CSM.SRV);
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		ReleaseCOM(CSM.DSV[i]);
	}
	ReleaseCOM(CSM.Texture);

	CSM.Resolution = InResolution;

	// Texture2DArray: ArraySize = MAX_SHADOW_CASCADES, R32_TYPELESS
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width  = InResolution;
	TexDesc.Height = InResolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = MAX_SHADOW_CASCADES;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage  = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &CSM.Texture);
	if (FAILED(hr)) return;

	// Per-cascade DSV
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateDepthStencilView(CSM.Texture, &DSVDesc, &CSM.DSV[i]);
	}

	// SRV — 전체 array (셰이더용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = MAX_SHADOW_CASCADES;

	Device->CreateShaderResourceView(CSM.Texture, &SRVDesc, &CSM.SRV);
}

void FShadowMapResources::EnsureSpotAtlas(ID3D11Device* Device, uint32 InResolution, uint32 InPageCount, uint32 MaxLights)
{
	if (InPageCount == 0) return;

	if (Spot.Resolution == InResolution && Spot.PageCount == InPageCount
		&& Spot.DataCapacity == MaxLights && Spot.Texture)
		return;

	// 기존 리소스 해제
	ReleaseCOM(Spot.SRV);
	ReleaseViewArray(Spot.DSVs);
	ReleaseCOM(Spot.Texture);
	ReleaseCOM(Spot.DataSRV);
	ReleaseCOM(Spot.DataBuffer);
	Spot.DataCapacity = 0;

	Spot.Resolution = InResolution;
	Spot.PageCount  = InPageCount;

	// Texture2DArray
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width  = InResolution;
	TexDesc.Height = InResolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = InPageCount;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage  = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &Spot.Texture);
	if (FAILED(hr)) return;

	// Per-page DSV
	Spot.DSVs.resize(InPageCount, nullptr);
	for (uint32 i = 0; i < InPageCount; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;

		Device->CreateDepthStencilView(Spot.Texture, &DSVDesc, &Spot.DSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = InPageCount;

	Device->CreateShaderResourceView(Spot.Texture, &SRVDesc, &Spot.SRV);

	// StructuredBuffer<FSpotShadowDataGPU>
	Spot.DataCapacity = MaxLights;

	D3D11_BUFFER_DESC BufDesc = {};
	BufDesc.ByteWidth = sizeof(FSpotShadowDataGPU) * MaxLights;
	BufDesc.Usage = D3D11_USAGE_DYNAMIC;
	BufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	BufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufDesc.StructureByteStride = sizeof(FSpotShadowDataGPU);

	Device->CreateBuffer(&BufDesc, nullptr, &Spot.DataBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC SBSRVDesc = {};
	SBSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SBSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SBSRVDesc.Buffer.NumElements = MaxLights;

	Device->CreateShaderResourceView(Spot.DataBuffer, &SBSRVDesc, &Spot.DataSRV);
}

void FShadowMapResources::EnsurePointAtlas(ID3D11Device* Device, uint32 AtlasSize, uint32 InPageCount, uint32 MaxLights)
{
	if (InPageCount == 0) return;

	if (Point.Resolution == AtlasSize && Point.PageCount == InPageCount
		&& Point.DataCapacity == MaxLights && Point.Texture)
		return;

	ReleaseViewArray(Point.DSVs);
	ReleaseCOM(Point.SRV);
	ReleaseCOM(Point.Texture);
	Point.Resolution = 0;
	Point.PageCount  = 0;

	ReleaseCOM(Point.DataSRV);
	ReleaseCOM(Point.DataBuffer);
	Point.DataCapacity = 0;

	if (AtlasSize == 0 || MaxLights == 0)
		return;

	Point.Resolution = AtlasSize;
	Point.PageCount  = InPageCount;

	// Texture2DArray atlas (R32_TYPELESS — depth + SRV)
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width            = AtlasSize;
	TexDesc.Height           = AtlasSize;
	TexDesc.MipLevels        = 1;
	TexDesc.ArraySize        = InPageCount;
	TexDesc.Format           = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage            = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&TexDesc, nullptr, &Point.Texture))) { assert(false); return; }

	// Per-page DSV
	Point.DSVs.resize(InPageCount, nullptr);
	for (uint32 i = 0; i < InPageCount; ++i)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice        = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize       = 1;
		Device->CreateDepthStencilView(Point.Texture, &DSVDesc, &Point.DSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format                           = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension                    = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MostDetailedMip   = 0;
	SRVDesc.Texture2DArray.MipLevels         = 1;
	SRVDesc.Texture2DArray.FirstArraySlice   = 0;
	SRVDesc.Texture2DArray.ArraySize         = InPageCount;
	Device->CreateShaderResourceView(Point.Texture, &SRVDesc, &Point.SRV);

	// StructuredBuffer<FPointShadowDataGPU>
	Point.DataCapacity = MaxLights;

	D3D11_BUFFER_DESC BufferDesc = {};
	BufferDesc.ByteWidth           = sizeof(FPointShadowDataGPU) * MaxLights;
	BufferDesc.Usage               = D3D11_USAGE_DYNAMIC;
	BufferDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
	BufferDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
	BufferDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(FPointShadowDataGPU);
	Device->CreateBuffer(&BufferDesc, nullptr, &Point.DataBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC BufSRVDesc = {};
	BufSRVDesc.Format             = DXGI_FORMAT_UNKNOWN;
	BufSRVDesc.ViewDimension      = D3D11_SRV_DIMENSION_BUFFER;
	BufSRVDesc.Buffer.NumElements = MaxLights;
	Device->CreateShaderResourceView(Point.DataBuffer, &BufSRVDesc, &Point.DataSRV);
}

// ============================================================
// FShadowMapResources — Ensure (VSM)
// ============================================================

void FShadowMapResources::EnsureCSM_VSM(ID3D11Device* Device, uint32 InResolution)
{
	if (CSM.Resolution == InResolution && CSM.VSMTexture) return;

	// 기존 VSM CSM 리소스 해제
	ReleaseCOM(CSM.VSMSRV);
	for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		ReleaseCOM(CSM.VSMRTV[i]);
		ReleaseCOM(CSM.VSMDSV[i]);
	}
	ReleaseCOM(CSM.VSMTexture);
	ReleaseCOM(CSM.VSMDepthTexture);

	// Moment Texture: R32G32_FLOAT, Texture2DArray
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width  = InResolution;
	MomentDesc.Height = InResolution;
	MomentDesc.MipLevels = 1;
	MomentDesc.ArraySize = MAX_SHADOW_CASCADES;
	MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage  = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &CSM.VSMTexture))) return;

	// Depth Texture: D32_FLOAT (DSV 전용, SRV 불필요)
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width  = InResolution;
	DepthDesc.Height = InResolution;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = MAX_SHADOW_CASCADES;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage  = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &CSM.VSMDepthTexture)))
	{
		CSM.VSMTexture->Release(); CSM.VSMTexture = nullptr;
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
		Device->CreateRenderTargetView(CSM.VSMTexture, &RTVDesc, &CSM.VSMRTV[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateDepthStencilView(CSM.VSMDepthTexture, &DSVDesc, &CSM.VSMDSV[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = MAX_SHADOW_CASCADES;
	Device->CreateShaderResourceView(CSM.VSMTexture, &SRVDesc, &CSM.VSMSRV);
}

void FShadowMapResources::EnsureSpotAtlas_VSM(ID3D11Device* Device, uint32 InResolution, uint32 InPageCount)
{
	if (InPageCount == 0) return;
	if (Spot.Resolution == InResolution && Spot.PageCount == InPageCount && Spot.VSMTexture)
		return;

	// 기존 Spot VSM 리소스 해제
	ReleaseCOM(Spot.VSMSRV);
	ReleaseViewArray(Spot.VSMRTVs);
	ReleaseViewArray(Spot.VSMDSVs);
	ReleaseCOM(Spot.VSMTexture);
	ReleaseCOM(Spot.VSMDepthTexture);

	// Moment Texture
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width  = InResolution;
	MomentDesc.Height = InResolution;
	MomentDesc.MipLevels = 1;
	MomentDesc.ArraySize = InPageCount;
	MomentDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage  = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &Spot.VSMTexture))) return;

	// Depth Texture
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width  = InResolution;
	DepthDesc.Height = InResolution;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = InPageCount;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage  = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &Spot.VSMDepthTexture)))
	{
		Spot.VSMTexture->Release(); Spot.VSMTexture = nullptr;
		return;
	}

	// Per-slice RTV + DSV
	Spot.VSMRTVs.resize(InPageCount, nullptr);
	Spot.VSMDSVs.resize(InPageCount, nullptr);
	for (uint32 i = 0; i < InPageCount; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateRenderTargetView(Spot.VSMTexture, &RTVDesc, &Spot.VSMRTVs[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize = 1;
		Device->CreateDepthStencilView(Spot.VSMDepthTexture, &DSVDesc, &Spot.VSMDSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = 1;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize = InPageCount;
	Device->CreateShaderResourceView(Spot.VSMTexture, &SRVDesc, &Spot.VSMSRV);
}

void FShadowMapResources::EnsurePointAtlas_VSM(ID3D11Device* Device, uint32 AtlasSize, uint32 InPageCount)
{
	if (InPageCount == 0) return;
	if (Point.Resolution == AtlasSize && Point.PageCount == InPageCount && Point.VSMTexture)
		return;

	ReleaseCOM(Point.VSMSRV);
	ReleaseViewArray(Point.VSMRTVs);
	ReleaseViewArray(Point.VSMDSVs);
	ReleaseCOM(Point.VSMTexture);
	ReleaseCOM(Point.VSMDepthTexture);

	if (AtlasSize == 0) return;

	// Moment atlas: R32G32_FLOAT Texture2DArray
	D3D11_TEXTURE2D_DESC MomentDesc = {};
	MomentDesc.Width            = AtlasSize;
	MomentDesc.Height           = AtlasSize;
	MomentDesc.MipLevels        = 1;
	MomentDesc.ArraySize        = InPageCount;
	MomentDesc.Format           = DXGI_FORMAT_R32G32_FLOAT;
	MomentDesc.SampleDesc.Count = 1;
	MomentDesc.Usage            = D3D11_USAGE_DEFAULT;
	MomentDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(Device->CreateTexture2D(&MomentDesc, nullptr, &Point.VSMTexture))) return;

	// Depth atlas: D32_FLOAT Texture2DArray
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width            = AtlasSize;
	DepthDesc.Height           = AtlasSize;
	DepthDesc.MipLevels        = 1;
	DepthDesc.ArraySize        = InPageCount;
	DepthDesc.Format           = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage            = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &Point.VSMDepthTexture)))
	{
		Point.VSMTexture->Release(); Point.VSMTexture = nullptr;
		return;
	}

	// Per-page RTV + DSV
	Point.VSMRTVs.resize(InPageCount, nullptr);
	Point.VSMDSVs.resize(InPageCount, nullptr);
	for (uint32 i = 0; i < InPageCount; ++i)
	{
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
		RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice        = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize       = 1;
		Device->CreateRenderTargetView(Point.VSMTexture, &RTVDesc, &Point.VSMRTVs[i]);

		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice        = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		DSVDesc.Texture2DArray.ArraySize       = 1;
		Device->CreateDepthStencilView(Point.VSMDepthTexture, &DSVDesc, &Point.VSMDSVs[i]);
	}

	// SRV — 전체 array
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format                         = DXGI_FORMAT_R32G32_FLOAT;
	SRVDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MostDetailedMip = 0;
	SRVDesc.Texture2DArray.MipLevels       = 1;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.ArraySize       = InPageCount;
	Device->CreateShaderResourceView(Point.VSMTexture, &SRVDesc, &Point.VSMSRV);
}

// ============================================================
// FShadowMapResources::Release
// ============================================================

void FShadowMapResources::Release()
{
	CSM.Release();
	Spot.Release();
	Point.Release();
}

// ============================================================
// FSystemResources
// ============================================================

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

	// Point lights — ShadowMapIndex는 ShadowMapPass::EndPass에서 패치
	for (uint32 i = 0; i < NumPointLights; ++i)
	{
		FLightInfo Info = Env.GetPointLight(i).ToLightInfo();
		Info.ShadowMapIndex = 0;
		Infos.emplace_back(Info);
	}

	// Spot lights — ShadowMapIndex는 ShadowMapPass::EndPass에서 패치
	for (uint32 i = 0; i < NumSpotLights; ++i)
	{
		FLightInfo Info = Env.GetSpotLight(i).ToLightInfo();
		Info.ShadowMapIndex = 0;
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
