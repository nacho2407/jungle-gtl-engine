#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "VertexSimple.h"
#include "Matrix.h"
#include "Vector.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "RenderObject.h"
#include "EngineTypes.h"
#include "ConstantBuffer.h"
#include "Level.h"


class URenderer {
public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11RasterizerState* RasterizerStateNoCull = nullptr;
	ID3D11DepthStencilState* DepthStencilState = nullptr;
	ID3D11DepthStencilState* DepthStencilStateNoWrite = nullptr;
	ID3D11DepthStencilState* DepthStencilStateNoDepth = nullptr;
	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	ID3D11BlendState* BlendState = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

public:
	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);
	void CreateBlendState();
	void ReleaseDeviceAndSwapChain();
	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateRasterizerState();
	void ReleaseRasterizerState();
	void CreateDepthStencilState();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
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
		int bIsSelected;
		float Padding[3];
	};

	struct FGridConstant
	{
		float CameraPos[3];
		float GridSize;
		float LineThickness;
		float Padding[3];
	};

	FConstantBuffer FrameConstantBuffer; //View Proj
	FConstantBuffer ObjectConstantBuffer; //World

	FConstantBuffer FGridConstantBuffer;

	void ReleaseConstantBuffer();
	void CreateConstantBuffer();


public:
	void Render(class UCamera* Camera);
	void RenderGrid(UCamera* Camera, ID3D11VertexShader* VertexShader, ID3D11PixelShader* PixelShader);
	FLevel drawLevel;
};
