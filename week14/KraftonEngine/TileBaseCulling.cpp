#include "TileBaseCulling.h"


static ID3D11ComputeShader* CompileCS(ID3D11Device* Dev, const wchar_t* Path, const char* Entry)
{
	ID3DBlob* csBlob  = nullptr;
	ID3DBlob* errBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(Path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		Entry, "cs_5_0", 0, 0, &csBlob, &errBlob);

	if (FAILED(hr))
	{
		if (errBlob)
		{
			MessageBoxA(nullptr, (char*)errBlob->GetBufferPointer(),
				"Compute Shader Compile Error", MB_OK | MB_ICONERROR);
			errBlob->Release();
		}
		return nullptr;
	}

	ID3D11ComputeShader* cs = nullptr;
	hr = Dev->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs);
	csBlob->Release();

	return SUCCEEDED(hr) ? cs : nullptr;
}

void FTileBaseCulling::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;

	TileLightCullingCS = CompileCS(Device, L"Shaders/TileLightCulling.hlsl", "mainCS");

	if (!TileLightCullingCS)
		return;

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(FTileCullingCBData);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&cbDesc, nullptr, &TileCullingCB);

	bInitialized = (TileCullingCB != nullptr);
}

void FTileBaseCulling::Release()
{
	if (TileLightCullingCS) { TileLightCullingCS->Release(); TileLightCullingCS = nullptr; }
	if (TileCullingCB)      { TileCullingCB->Release();      TileCullingCB = nullptr; }

	bInitialized = false;
}

void FTileBaseCulling::Dispatch(
	ID3D11DeviceContext* Ctx,
	const FFrameContext& Frame,
	ID3D11Buffer*        FrameCB,
	FTileCullingResource& TileCulling,
	ID3D11ShaderResourceView* AllLightsSRV,
	uint32 NumLights,
	uint32 ViewportWidth,
	uint32 ViewportHeight)
{
	if (!bInitialized) return;

	// ── Step 1: 뷰포트 크기 바뀌면 타일 버퍼 재생성 ──────────────────
	const uint32 TileCountX = (ViewportWidth  + ETileCulling::TileSize - 1) / ETileCulling::TileSize;
	const uint32 TileCountY = (ViewportHeight + ETileCulling::TileSize - 1) / ETileCulling::TileSize;

	if (ViewportWidth != CachedViewportWidth || ViewportHeight != CachedViewportHeight)
	{
		TileCulling.Create(Device, TileCountX, TileCountY);
		CachedViewportWidth  = ViewportWidth;
		CachedViewportHeight = ViewportHeight;
	}

	// ── Step 2: CB 업데이트 ───────────────────────────────────────────
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		Ctx->Map(TileCullingCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);

		FTileCullingCBData* CB = reinterpret_cast<FTileCullingCBData*>(Mapped.pData);
		CB->ScreenSizeX      = ViewportWidth;
		CB->ScreenSizeY      = ViewportHeight;
		CB->Enable25DCulling = 1;
		CB->NearZ            = Frame.NearClip;
		CB->FarZ             = Frame.FarClip;
		CB->NumLights        = NumLights;

		Ctx->Unmap(TileCullingCB, 0);
	}

	// ── Step 3: 버퍼 초기화 (매 프레임 클리어) ───────────────────────
	// GlobalLightCounter, TileLightGrid, TileLightIndices 모두 클리어
	const UINT Zeros[4] = {};
	Ctx->ClearUnorderedAccessViewUint(TileCulling.CounterUAV, Zeros);
	Ctx->ClearUnorderedAccessViewUint(TileCulling.GridUAV,    Zeros);
	Ctx->ClearUnorderedAccessViewUint(TileCulling.IndicesUAV, Zeros);

	// ── Step 4: CS 리소스 바인딩 ──────────────────────────────────────
	// b0: Frame CB (View, Projection — CS 슬롯은 VS/PS와 독립적으로 바인딩 필요)
	// b2: TileCulling CB (ScreenSize, NearZ, FarZ, NumLights)
	Ctx->CSSetConstantBuffers(0, 1, &FrameCB);
	Ctx->CSSetConstantBuffers(2, 1, &TileCullingCB);
	Ctx->CSSetShaderResources(0, 1, &Frame.DepthCopySRV);
	Ctx->CSSetShaderResources(8, 1, &AllLightsSRV);

	ID3D11UnorderedAccessView* UAVs[3] = {
		TileCulling.IndicesUAV,
		TileCulling.GridUAV,
		TileCulling.CounterUAV
	};
	Ctx->CSSetUnorderedAccessViews(0, 3, UAVs, nullptr);

	// ── Step 5: Dispatch ──────────────────────────────────────────────
	Ctx->CSSetShader(TileLightCullingCS, nullptr, 0);
	Ctx->Dispatch(TileCountX, TileCountY, 1);

	// ── Step 6: 언바인딩 ─────────────────────────────────────────────
	ID3D11Buffer*              NullCBs[3]  = {};
	ID3D11ShaderResourceView*  NullSRVs[9] = {};
	ID3D11UnorderedAccessView* NullUAVs[3] = {};
	Ctx->CSSetConstantBuffers(0, 3, NullCBs);
	Ctx->CSSetShaderResources(0, 9, NullSRVs);
	Ctx->CSSetUnorderedAccessViews(0, 3, NullUAVs, nullptr);
	Ctx->CSSetShader(nullptr, nullptr, 0);
}
