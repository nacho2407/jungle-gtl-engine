#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "FVertexSimple.h"
#include "FMatrix.h"
#include "FVector.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "RenderObject.h"
#include "UEngineTypes.h"
#include "FConstantBuffer.h"


class URenderer {
public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11DepthStencilState* DepthStencilState = nullptr;
	ID3D11DepthStencilState* DepthStencilStateNoDepth = nullptr;
	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

public:
	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);
	void ReleaseDeviceAndSwapChain();
	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateRasterizerState();
	void ReleaseRasterizerState();
	void CreateDepthStencilState();
	void ReleaseDepthStencilState();
	void Release();
	void SwapBuffer();
	void OnResize(UINT Width, UINT Height);

public:

	void Prepare();

public:
	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);


public:
	struct FFrameConstant
	{
		FMatrix View;
		FMatrix Projection;
	};
	struct FObjectConstant
	{
		FMatrix World;
		bool bIsSelected;
	};
	FConstantBuffer FrameConstantBuffer; //View Proj
	FConstantBuffer ObjectConstantBuffer; //World

	void ReleaseConstantBuffer();
	void CreateConstantBuffer();


public:

	void PushRenderObject(URenderObject* RenderObj) { RenderObjects.push_back(RenderObj); }
	void ClearRenderObjects();
	void Render(class UCamera* Camera);

	TArray<URenderObject*> RenderObjects;
};
