#pragma once

#include "ConstantBuffer.h"
#include "Level.h"
#include "Math/Matrix.h"
#include "VertexSimple.h"
#include <d3d11.h>
#include "EngineCore/EngineTypes.h"
#include "EngineCore/Color.h"
#include "Component/CameraComponent.h"
#include "Renderer/Material.h"
#include "Renderer/Geometry.h"

#include "Renderer/VertexFont.h"
#include "Text/FontAsset.h"

/**
 * @brief Billboard 텍스트 배치 렌더링 시 한 번에 그릴 단위인 배치 그룹을 설명
 */
struct FBillboardTextBatchKey
{
	FFontAsset* FontAsset = nullptr;
	FColor Color{};

	bool operator<(const FBillboardTextBatchKey& Other) const
	{
		return std::tie(FontAsset, Color.r, Color.g, Color.b, Color.a)
			< std::tie(Other.FontAsset, Other.Color.r, Other.Color.g, Other.Color.b, Other.Color.a);
	}

	// 해시 맵 사용을 위한 동등성 검사
	bool operator==(const FBillboardTextBatchKey& Other) const
	{
		return FontAsset == Other.FontAsset
			&& Color.r == Other.Color.r
			&& Color.g == Other.Color.g
			&& Color.b == Other.Color.b
			&& Color.a == Other.Color.a;
	}
};

/**
 * @brief 해시 지원을 위한 functor
 */
struct FBillboardTextBatchKeyHash
{
	size_t operator()(const FBillboardTextBatchKey& Key) const
	{
		// (참고) Boost 라이브러리에서 자주 사용하는 패턴
		size_t Seed = std::hash<FFontAsset*>{}(Key.FontAsset);
		const auto HashCombine = [&Seed](size_t Value)
			{
				Seed ^= Value + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
			};

		HashCombine(std::hash<float>{}(Key.Color.r));
		HashCombine(std::hash<float>{}(Key.Color.g));
		HashCombine(std::hash<float>{}(Key.Color.b));
		HashCombine(std::hash<float>{}(Key.Color.a));
		return Seed;
	}
};

/**
 * @brief 텍스트 Batch용 입력 구조체
 */
struct FBillboardTextDrawItem
{
	FFontAsset* FontAsset = nullptr;
	FString Text; // 실제 배치할 문자열 내용
	FVector Anchor{}; // 텍스트 전체 배치의 시작 기준점
	float Scale = 0.0f; // Glyph 크기 배율
	float LineGap = 0.0f; // 줄바꿈 시 줄 간격
	FColor Color{}; // 텍스트 색
};

class URenderer {
public:
	TComPtr<ID3D11Device> Device = nullptr;
	TComPtr<ID3D11DeviceContext> DeviceContext = nullptr;
	TComPtr<IDXGISwapChain> SwapChain = nullptr;

	TComPtr<ID3D11Texture2D> FrameBuffer = nullptr;
	TComPtr<ID3D11RenderTargetView> FrameBufferRTV = nullptr;
	TComPtr<ID3D11Texture2D> DepthStencilBuffer = nullptr;
	TComPtr<ID3D11DepthStencilView> DepthStencilView = nullptr;

	TComPtr<ID3D11RasterizerState> RasterizerState = nullptr;
	TComPtr<ID3D11RasterizerState> RasterizerStateNoCull = nullptr;
	TComPtr<ID3D11RasterizerState> RasterizerStateWireframe = nullptr;

	TComPtr<ID3D11DepthStencilState> DepthStencilState = nullptr;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateNoWrite = nullptr;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateNoDepth = nullptr;
	TComPtr<ID3D11BlendState> BlendState = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

	int DynamicBufferMaxVertexCount = 80000;
	int DrawCallCount = 0;

public:
	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);
	void CreateFrameBuffer();
	void CreateRasterizerState();
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateConstantBuffer();

	void ReleaseDeviceAndSwapChain();
	void ReleaseFrameBuffer();
	void ReleaseRasterizerState();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseConstantBuffer();
	void Release();

	void SwapBuffer();
	void OnResize(UINT Width, UINT Height);

public:


	//Dynamic Batching Test
	void CreateDynamicVertexBuffer();
	void UpdateDynamicVertexBuffer(FVertexSimple* vertices, UINT vertexCount);
	void ReleaseDynamicVertexBuffer();
	TComPtr<ID3D11Buffer> DynamicVertexBuffer = nullptr;
	void Prepare();


public:
	struct FFrameConstant
	{
		FMatrix View;
		FMatrix Projection;
	};

	FConstantBuffer FrameConstantBuffer; //View Proj


public:
	void SetRenderCamera(class UCameraComponent* camera);
	void RenderNoDepthObjects(TArray<RenderObject*>& NoDepthRenderObjects);
	void RenderNormalObjects(TArray<RenderObject*>& NormalRenderObjects);
	void DistributeRenderObjects(
		const TArray<FLevel*>& Levels,
		TMap<BatchKey, TArray<RenderObject*>, FBatchKeyHash>& BatchGroups,
		TArray<RenderObject*>& NormalRenderObjects,
		TArray<RenderObject*>& NoDepthRenderObjects
	);
	void BatchRender(TMap<BatchKey, TArray<RenderObject*>, FBatchKeyHash>& BatchGroups);
	void Render(const TArray<FLevel*>& Levels);
	void RenderTextMeshes(const TArray<FLevel*>& Levels);
	//void RenderGrid(ID3D11VertexShader* VertexShader, ID3D11PixelShader* PixelShader);
	void BatchRenderDebugLine(const TArray<FVertexSimple>& Lines);
	void SetRasterizerMode(int32 mode);

	TMap<EEngineShowFlags, bool> ShowFlagMap;

public:
	/**
	 * @brief 폰트를 위한 dynamic vertex buffer를 생성합니다.
	 */
	void CreateDynamicFontVertexBuffer(void);

	/**
	 * @brief 폰트를 위한 dynamic vertex buffer를 업데이트합니다.
	 *
	 * @param Vertices 업데이트할 폰트 정점 정보
	 *
	 * @param VectexCount 폰트 정점 개수
	 */
	void UpdateDynamicFontVertexBuffer(FVertexFont* Vertices, UINT VertexCount);

	/**
	 * @brief 폰트를 위한 dynamic vertex buffer를 해제합니다.
	 */
	void ReleaseDynamicFontVertexBuffer(void);

	/**
	 * @brief 빌보드 텍스트 draw item들을 배치 그룹별로 묶어 렌더링합니다.
	 *
	 * @param Items 이번 프레임에 렌더할 빌보드 텍스트 목록
	 */
	void RenderBillboardTextBatch(const TArray<FBillboardTextDrawItem>& Items);

private:
	UCameraComponent* renderCamera = nullptr;
	int32 RsSetMode = 0;

	/**
	 * @brief 폰트를 위한 dynamic vertex buffer
	 */
	TComPtr<ID3D11Buffer> DynamicFontVertexBuffer;

private:
	/**
	 * @brief GPU 상태를 텍스트 렌더링용에 맞추어 draw call을 준비합니다.
	 *
	 * @param FontAsset GPU에 설정할 font asset 정보
	 *
	 * @param Color GPU에 설정할 글자 색 정보
	 *
	 * @return 준비 성공 여부
	 */
	bool PrepareBillboardFontMaterial(FFontAsset* FontAsset, const FColor& Color);

	/**
	 * @brief 준비된 정점 배열로 실제 draw call을 보냅니다.
	 *
	 * @param Vertices Draw할 정점 배열
	 */
	void DrawBillboardFontVertices(const TArray<FVertexFont>& Vertices);

	/**
	 * @brief 현재 렌더러 설정에 맞는 rasterizer state를 복원합니다.
	 */
	void RestoreCurrentRasterizerState() const;
};
