#pragma once

#include <memory>

#include <d3d11.h>
#include <d3dcompiler.h>

// From DirectXTK
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "SimpleMath.h"

#include "Constants.h"
#include "Core.h"

#include "UTexture.hpp"

class URenderer
{
public:
	struct FConstants
	{
		// 16B
		FVector Offset;
		float Padding1;

		// 16B
		FVector Scale;
		float Padding2;

		// 16B
		FVector SinCos;
		float UseTexture;   // 1 = texture, 0 = color

		FVector TintColor;
		float Padding3;
	};

	struct FMesh
	{
		ID3D11Buffer* VertexBuffer = nullptr;
		UINT NumVertices;
	};

	FMesh sphere;
	FMesh rectangle;

	ID3D11Buffer* LineVertexBuffer = nullptr;

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11RasterizerState* RasterizerState = nullptr;

	D3D11_VIEWPORT ViewportInfo;

	ID3D11InputLayout* SimpleInputLayout = nullptr;
	unsigned int Stride;

	ID3D11VertexShader* SimpleVertexShader = nullptr;

	ID3D11PixelShader* SimplePixelShader = nullptr;

	ID3D11Buffer* ConstantBuffer = nullptr;

	ID3D11BlendState* AlphaBlendState = nullptr;
	// 화면 초기화 색상
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

	std::unique_ptr<DirectX::SpriteBatch> SpriteBatch;
	std::unique_ptr<DirectX::SpriteFont> Font;

public:
	void Create(HWND hWindow);

	void CreateDeviceAndSwapChain(HWND hWindow);

	void CreateSceneRenderTarget(UINT width, UINT height);

	void CreateFrameBuffer();

	void CreateRasterizerState();

	void CreateSphereMesh();

	void CreateRectangleMesh();

	void CreateLine();

	void CreateFont();

	void Release();

	void ReleaseLine();

	void ReleaseFrameBuffer();

	void ReleaseDeviceAndSwapChain();

	void ReleaseRasterizerState();

	void ReleaseSphereMesh();

	void ReleaseRectangleMesh();

	void CreateShader();

	void ReleaseShader();

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);

	void ReleaseVertexBuffer(ID3D11Buffer*& vertexBuffer);

	void CreateConstantBuffer();

	void ReleaseConstantBuffer();

	void UpdateConstant(FVector Offset, float Angle, FVector Scale, float UseTexture, FVector Tint);

	void UpdateTexture(UTexture* InTexture);

	void Prepare();

	void PrepareShader();

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);

	void RenderSphere();

	void RenderRectangle();

	void RenderLine();

	void UpdateLineColor(bool collision);

	void SwapBuffer();
};
