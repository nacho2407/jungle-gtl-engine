#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>

#include "World/FScene.h"
#include "Math/FMatrix.h"
#include "Structs.h"
#include "Geometry/Sphere.h"
#include "Geometry/Axies.h"

class UCameraComponent;

class URenderer
{
public:
	URenderer();
	~URenderer();

public:
	bool Create(HWND hWindow);
	void Release();

	void BeginFrame();
	void Render(const FScene& Scene, const UCameraComponent* Camera);
	void EndFrame();

	void Resize(UINT width, UINT Height);

public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr; // @@@ 스텐실 버퍼 뭐하는 놈임?
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	ID3D11RasterizerState* RasterizerState = nullptr;

	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader* SimplePixelShader = nullptr;
	ID3D11InputLayout* SimpleInputLayout = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo = {};

	UINT Stride = sizeof(FVertexSimple); // @@@ 이게 필요한거임???

private:
    bool CreateDeviceAndSwapChain(HWND hWindow);
    bool CreateFrameBuffer();
    bool CreateDepthStencilBuffer(UINT Width, UINT Height);
    bool CreateRasterizerState();
    bool CreateShader(const wchar_t* ShaderPath);
    bool CreateConstantBuffer();

    void ReleaseFrameBuffer();
    void ReleaseDepthStencilBuffer();
    void ReleaseRasterizerState();
    void ReleaseShader();
    void ReleaseConstantBuffer();
    void ReleaseDeviceAndSwapChain();

    void PreparePipeline();
    void UpdateVSConstants(const FMatrix& World, const FMatrix& View, const FMatrix& Projection, const FVector4& Color);

private:
    bool CreateBuiltinGeometry();
    void ReleaseBuiltinGeometry();

    // @@@ 이거 각각에 대해서...다 만들어주어야 하는거임??
    // @@@ RenderItem Draw랑 Sphere Draw가 다른거임??? 역할이??? 뭐지???
    void DrawRenderItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);
    void DrawSphere(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);
    void DrawCube(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);
    void DrawStaticMesh(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);

    bool CreateAxesGeometry();
    void DrawAxes(const FMatrix& View, const FMatrix& Projection);

private:
    ID3D11Buffer* CubeVertexBuffer = nullptr;
    UINT CubeVertexCount = 0; // @@@ Count가 왜 필요하지?

    ID3D11Buffer* SphereVertexBuffer = nullptr;
    UINT SphereVertexCount = 0;

    ID3D11Buffer* AxesVertexBuffer = nullptr;
    UINT AxesVertexCount = 0;

private:
    struct FVSConstants
    {
        FMatrix World;
        FMatrix View;
        FMatrix Projection;
        FVector4 Color;
    };
};