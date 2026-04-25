#include "Engine/Render/Culling/TileBasedLightCulling.h"
#include "Engine/Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/World.h"

// ════════════════════════════════════════════════════════════════
// FTileCullingVisualizer
// ════════════════════════════════════════════════════════════════

void FTileCullingVisualizer::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;

	// CB (b3)
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth      = sizeof(FVisualizeCullingCBData);
	cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&cbDesc, nullptr, &VisCullingCB);

	// GPU StructuredBuffer (u3)
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.ByteWidth           = kElementCount * sizeof(FVector4);
	bufDesc.Usage               = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
	bufDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufDesc.StructureByteStride = sizeof(FVector4);
	Device->CreateBuffer(&bufDesc, nullptr, &VisBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format             = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = kElementCount;
	Device->CreateUnorderedAccessView(VisBuffer, &uavDesc, &VisBufferUAV);

	// Staging (CPU read)
	D3D11_BUFFER_DESC stagingDesc = {};
	stagingDesc.ByteWidth      = kElementCount * sizeof(FVector4);
	stagingDesc.Usage          = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	Device->CreateBuffer(&stagingDesc, nullptr, &VisStagingBuf);

	ReadbackData_.resize(kElementCount);
}

void FTileCullingVisualizer::Release()
{
	if (VisCullingCB)  { VisCullingCB->Release();  VisCullingCB = nullptr; }
	if (VisBufferUAV)  { VisBufferUAV->Release();  VisBufferUAV = nullptr; }
	if (VisBuffer)     { VisBuffer->Release();     VisBuffer = nullptr; }
	if (VisStagingBuf) { VisStagingBuf->Release(); VisStagingBuf = nullptr; }
}

void FTileCullingVisualizer::PreDispatch(ID3D11DeviceContext* Ctx, const FFrameContext& Frame, bool bVisualize)
{
	bCurrentFrameActive = bVisualize;
	bDispatchedThisFrame = true;

	// CB 업데이트
	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	Ctx->Map(VisCullingCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);

	FVisualizeCullingCBData* VCB = reinterpret_cast<FVisualizeCullingCBData*>(Mapped.pData);
	VCB->Visualize25DCulling = bVisualize ? 1 : 0;
	VCB->CursorTileX         = Frame.CursorViewportX / ETileCulling::TileSize;
	VCB->CursorTileY         = Frame.CursorViewportY / ETileCulling::TileSize;
	VCB->_pad                = 0.0f;
	FMatrix Inv = Frame.View.GetInverse();
	memcpy(VCB->InvView, Inv.Data, sizeof(float) * 16);

	Ctx->Unmap(VisCullingCB, 0);

	// UAV 클리어
	if (bVisualize && VisBufferUAV)
	{
		const UINT Zeros[4] = {};
		Ctx->ClearUnorderedAccessViewUint(VisBufferUAV, Zeros);
	}
}

void FTileCullingVisualizer::PostDispatch(ID3D11DeviceContext* Ctx, bool bVisualize)
{
	if (bVisualize && VisBuffer && VisStagingBuf)
	{
		Ctx->CopyResource(VisStagingBuf, VisBuffer);
		bPendingReadback = true;
	}
	else
	{
		bDataReady = false;
	}
}

void FTileCullingVisualizer::ReadbackData(ID3D11DeviceContext* Ctx)
{
	if (!bPendingReadback || !VisStagingBuf) return;

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	HRESULT hr = Ctx->Map(VisStagingBuf, 0, D3D11_MAP_READ, 0, &Mapped);
	if (SUCCEEDED(hr))
	{
		memcpy(ReadbackData_.data(), Mapped.pData, kElementCount * sizeof(FVector4));
		Ctx->Unmap(VisStagingBuf, 0);
		bDataReady = true;
	}
	bPendingReadback = false;
}

void FTileCullingVisualizer::SubmitDebugLines(ID3D11DeviceContext* Ctx, UWorld* World)
{
	// 이전 프레임에 Dispatch가 호출됐는지 확인 후 플래그 리셋
	bDispatchedLastFrame = bDispatchedThisFrame;
	bDispatchedThisFrame = false;

	ReadbackData(Ctx);

	// 이전 프레임에 Dispatch가 ���출되지 않았으면 (Cluster ���드 등)
	// stale bDataReady로 매 프레임 라인이 누적되는 것을 방지
	if (!bDispatchedLastFrame)
	{
		bDataReady = false;
		return;
	}

	if (!bDataReady || !World) return;

	constexpr uint32 kBoundaries = 33; // NUM_SLICES + 1

	// 각 슬라이스 경계에 사각형(quad) 외곽선
	for (uint32 b = 0; b < kBoundaries; b++)
	{
		uint32 off = b * 4;
		if (ReadbackData_[off].W == 0.0f) continue;

		FVector TL(ReadbackData_[off + 0].X, ReadbackData_[off + 0].Y, ReadbackData_[off + 0].Z);
		FVector TR(ReadbackData_[off + 1].X, ReadbackData_[off + 1].Y, ReadbackData_[off + 1].Z);
		FVector BL(ReadbackData_[off + 2].X, ReadbackData_[off + 2].Y, ReadbackData_[off + 2].Z);
		FVector BR(ReadbackData_[off + 3].X, ReadbackData_[off + 3].Y, ReadbackData_[off + 3].Z);

		// 8슬라이스 간격마다 노란색, 나머지 시안
		FColor Color = (b % 8 == 0) ? FColor::Yellow() : FColor(0, 255, 255);
		DrawDebugLine(World, TL, TR, Color);
		DrawDebugLine(World, TR, BR, Color);
		DrawDebugLine(World, BR, BL, Color);
		DrawDebugLine(World, BL, TL, Color);
	}

	// 코너끼리 수직 연결선 (슬라이스 깊이 방향)
	for (uint32 c = 0; c < 4; c++)
	{
		for (uint32 b = 0; b < kBoundaries - 1; b++)
		{
			uint32 off0 = b * 4 + c;
			uint32 off1 = (b + 1) * 4 + c;
			if (ReadbackData_[off0].W == 0.0f || ReadbackData_[off1].W == 0.0f) continue;

			FVector P0(ReadbackData_[off0].X, ReadbackData_[off0].Y, ReadbackData_[off0].Z);
			FVector P1(ReadbackData_[off1].X, ReadbackData_[off1].Y, ReadbackData_[off1].Z);
			DrawDebugLine(World, P0, P1, FColor::Green());
		}
	}
}

// ════════════════════════════════════════════════════════════════
// FTileBasedLightCulling
// ════════════════════════════════════════════════════════════════

void FTileBasedLightCulling::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;

	TileLightCullingCS = FShaderManager::Get().GetOrCreateCS(
		"Shaders/Lighting/TileLightCulling.hlsl", "mainCS");

	if (!TileLightCullingCS || !TileLightCullingCS->IsValid())
		return;

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth      = sizeof(FTileLightCullingCBData);
	cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&cbDesc, nullptr, &TileCullingCB);

	Visualizer.Initialize(Device);

	bInitialized = (TileCullingCB != nullptr);
}

void FTileBasedLightCulling::Release()
{
	TileLightCullingCS = nullptr;  // FShaderManager가 소유
	if (TileCullingCB) { TileCullingCB->Release(); TileCullingCB = nullptr; }

	Visualizer.Release();

	bInitialized = false;
}

void FTileBasedLightCulling::Dispatch(
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

	ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
	Ctx->PSSetShaderResources(9, 2, NullSRVs);

	// ── Step 1: 뷰포트 크기 바뀌면 타일 버퍼 재생성 ──────────────────
	const uint32 TileCountX = (ViewportWidth  + ETileCulling::TileSize - 1) / ETileCulling::TileSize;
	const uint32 TileCountY = (ViewportHeight + ETileCulling::TileSize - 1) / ETileCulling::TileSize;

	if (ViewportWidth != CachedViewportWidth || ViewportHeight != CachedViewportHeight)
	{
		TileCulling.Create(Device, TileCountX, TileCountY);
		CachedViewportWidth  = ViewportWidth;
		CachedViewportHeight = ViewportHeight;
	}

	// ── Step 2: TileCulling CB (b2) 업데이트 ─────────────────────────
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		Ctx->Map(TileCullingCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);

		FTileLightCullingCBData* CB = reinterpret_cast<FTileLightCullingCBData*>(Mapped.pData);
		CB->ScreenSizeX      = ViewportWidth;
		CB->ScreenSizeY      = ViewportHeight;
		CB->Enable25DCulling = Frame.RenderOptions.Enable25DCulling;
		CB->NearZ            = Frame.NearClip;
		CB->FarZ             = Frame.FarClip;
		CB->NumLights        = NumLights;
		CB->_pad[0] = CB->_pad[1] = 0.0f;

		Ctx->Unmap(TileCullingCB, 0);
	}

	// ── Step 3: Visualizer CB (b3) + UAV 준비 ────────────────────────
	const bool bVisualize = Frame.RenderOptions.ShowFlags.bVisualize25DCulling
		&& Frame.CursorViewportX != UINT32_MAX;
	Visualizer.PreDispatch(Ctx, Frame, bVisualize);

	// ── Step 4: 타일 버퍼 초기화 ─────────────────────────────────────
	const UINT Zeros[4] = {};
	Ctx->ClearUnorderedAccessViewUint(TileCulling.CounterUAV, Zeros);
	Ctx->ClearUnorderedAccessViewUint(TileCulling.GridUAV,    Zeros);
	Ctx->ClearUnorderedAccessViewUint(TileCulling.IndicesUAV, Zeros);

	// ── Step 5: CS 리소스 바인딩 ─────────────────────────────────────
	Ctx->CSSetConstantBuffers(0, 1, &FrameCB);
	Ctx->CSSetConstantBuffers(2, 1, &TileCullingCB);
	ID3D11Buffer* visCB = Visualizer.GetCB();
	Ctx->CSSetConstantBuffers(3, 1, &visCB);
	Ctx->CSSetShaderResources(0, 1, &Frame.DepthCopySRV);
	Ctx->CSSetShaderResources(8, 1, &AllLightsSRV);

	if (Visualizer.IsActive() && Visualizer.GetBufferUAV())
	{
		ID3D11UnorderedAccessView* UAVs[4] = {
			TileCulling.IndicesUAV,
			TileCulling.GridUAV,
			TileCulling.CounterUAV,
			Visualizer.GetBufferUAV()
		};
		Ctx->CSSetUnorderedAccessViews(0, 4, UAVs, nullptr);
	}
	else
	{
		ID3D11UnorderedAccessView* UAVs[3] = {
			TileCulling.IndicesUAV,
			TileCulling.GridUAV,
			TileCulling.CounterUAV
		};
		Ctx->CSSetUnorderedAccessViews(0, 3, UAVs, nullptr);
	}

	// ── Step 6: Dispatch ─────────────────────────────────────────────
	TileLightCullingCS->Bind(Ctx);
	Ctx->Dispatch(TileCountX, TileCountY, 1);

	// ── Step 7: 언바인딩 + 시각화 readback 요청 ─────────────────────
	ID3D11UnorderedAccessView* NullUAVs[4] = {};
	Ctx->CSSetUnorderedAccessViews(0, 4, NullUAVs, nullptr);
	Ctx->CSSetShader(nullptr, nullptr, 0);

	Visualizer.PostDispatch(Ctx, bVisualize);
}
