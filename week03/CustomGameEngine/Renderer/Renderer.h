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
		TMap<BatchKey, TArray<RenderObject*>>& BatchGroups,
		TArray<RenderObject*>& NormalRenderObjects,
		TArray<RenderObject*>& NoDepthRenderObjects
	);
	void BatchRender(TMap<BatchKey, TArray<RenderObject*>>& BatchGroups);
	void Render(const TArray<FLevel*>& Levels);
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
	 * @brief 일단 빌보드용 Render Path가 살아있는지 확인하기 위한 테스트 함수입니다.
	 */
	void RenderSingleBillboardTextTest(void);

private:
	UCameraComponent* renderCamera = nullptr;
	int32 RsSetMode = 0;

	/**
	 * @brief 폰트를 위한 dynamic vertex buffer
	 */
	TComPtr<ID3D11Buffer> DynamicFontVertexBuffer;
};
