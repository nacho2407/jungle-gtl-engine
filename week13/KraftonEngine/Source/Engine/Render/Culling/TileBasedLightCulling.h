#pragma once

#include "Engine/Render/Types/ForwardLightData.h"
#include "Engine/Render/Types/FrameContext.h"

struct FTileCullingResource;

class FComputeShader;


// b2: 타일 컬링 파라미터
struct FTileLightCullingCBData
{
	uint32 ScreenSizeX;
	uint32 ScreenSizeY;
	uint32 Enable25DCulling;
	float  NearZ;
	float  FarZ;
	uint32 NumLights;
	float  _pad[2];
};

// b3: 시각화 전용 파라미터
struct FVisualizeCullingCBData
{
	uint32 Visualize25DCulling;
	uint32 CursorTileX;
	uint32 CursorTileY;
	float  _pad;
	float  InvView[4][4];  // FMatrix 직접 쓰면 __m256 정렬로 패딩 발생
};

// ============================================================
// FTileCullingVisualizer — 2.5D Culling 슬라이스 시각화 전담
// ============================================================
class FTileCullingVisualizer
{
public:
	FTileCullingVisualizer() = default;
	~FTileCullingVisualizer() { Release(); }

	FTileCullingVisualizer(const FTileCullingVisualizer&) = delete;
	FTileCullingVisualizer& operator=(const FTileCullingVisualizer&) = delete;

	void Initialize(ID3D11Device* InDevice);
	void Release();

	// Dispatch 전: CB 업데이트 + UAV 클리어
	void PreDispatch(ID3D11DeviceContext* Ctx, const FFrameContext& Frame, bool bVisualize);

	// Dispatch 후: GPU→Staging 복사 요청
	void PostDispatch(ID3D11DeviceContext* Ctx, bool bVisualize);

	// 이전 프레임 결과 readback (CPU 매핑)
	void ReadbackData(ID3D11DeviceContext* Ctx);

	// readback + 디버그 라인 제출 (매 프레임 Execute 초반에 호출)
	void SubmitDebugLines(ID3D11DeviceContext* Ctx, class UWorld* World);

	// 상태 접근
	bool IsDataReady() const { return bDataReady; }
	const TArray<FVector4>& GetReadbackData() const { return ReadbackData_; }

	// Dispatch 시 바인딩에 필요한 리소스
	ID3D11Buffer*              GetCB()       const { return VisCullingCB; }
	ID3D11UnorderedAccessView* GetBufferUAV() const { return VisBufferUAV; }
	bool                       IsActive()    const { return bCurrentFrameActive; }

private:
	static constexpr uint32 kCornersPerTile = 4 * 33; // 4 corners * 33 boundaries
	static constexpr uint32 kElementCount   = kCornersPerTile;

	ID3D11Device* Device = nullptr;

	ID3D11Buffer*              VisCullingCB   = nullptr;
	ID3D11Buffer*              VisBuffer      = nullptr;
	ID3D11UnorderedAccessView* VisBufferUAV   = nullptr;
	ID3D11Buffer*              VisStagingBuf  = nullptr;

	TArray<FVector4> ReadbackData_;
	bool bDataReady          = false;
	bool bPendingReadback    = false;
	bool bCurrentFrameActive = false;
	bool bDispatchedThisFrame = false;
	bool bDispatchedLastFrame = false;
};

// ============================================================
// FTileBasedLightCulling — 타일 기반 라이트 컬링 CS
// ============================================================
class FTileBasedLightCulling
{
public:
	FTileBasedLightCulling() = default;
	~FTileBasedLightCulling() { Release(); }

	FTileBasedLightCulling(const FTileBasedLightCulling&) = delete;
	FTileBasedLightCulling& operator=(const FTileBasedLightCulling&) = delete;

	void Initialize(ID3D11Device* InDevice);
	void Release();

	bool IsInitialized() const { return bInitialized; }

	void Dispatch(
		ID3D11DeviceContext* Ctx,
		const FFrameContext& Frame,
		ID3D11Buffer*        FrameCB,
		FTileCullingResource& TileCulling,
		ID3D11ShaderResourceView* AllLightsSRV,
		uint32 NumLights,
		uint32 ViewportWidth,
		uint32 ViewportHeight);

	// 시각화: readback + 디버그 라인 제출
	void SubmitVisualizationDebugLines(ID3D11DeviceContext* Ctx, class UWorld* World)
	{
		Visualizer.SubmitDebugLines(Ctx, World);
	}

private:
	ID3D11Device* Device = nullptr;

	FComputeShader* TileLightCullingCS = nullptr;  // FShaderManager 소유
	ID3D11Buffer*        TileCullingCB      = nullptr;

	bool bInitialized = false;

	uint32 CachedViewportWidth  = 0;
	uint32 CachedViewportHeight = 0;

	FTileCullingVisualizer Visualizer;
};
