#pragma once

#include "ConstantBuffer.h"
#include "Level.h"
#include "Math/Matrix.h"
#include "VertexSimple.h"
#include <d3d11.h>
#include "EngineTypes.h"

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

	void Prepare();


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


public:
	void Render(class UCameraComponent* Camera);
	void RenderGrid(UCameraComponent* Camera, ID3D11VertexShader* VertexShader, ID3D11PixelShader* PixelShader);
	FLevel drawLevel;
};
