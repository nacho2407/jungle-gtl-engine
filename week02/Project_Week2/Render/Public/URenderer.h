#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <unordered_map>

#include "World/FScene.h"
#include "Math/FMatrix.h"
#include "Structs.h"
#include "Resource/MeshTypes.h"

class UStaticMesh;
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

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	ID3D11RasterizerState* RasterizerState = nullptr;

	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader* SimplePixelShader = nullptr;
	ID3D11InputLayout* SimpleInputLayout = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo = {};

	UINT Stride = sizeof(FVertexSimple);

private:
    struct FVSConstants
    {
        FMatrix World;
        FMatrix View;
        FMatrix Projection;
        FVector4 Color;
    };

    struct FMeshGPUResource
    {
        ID3D11Buffer* VertexBuffer = nullptr;
        ID3D11Buffer* IndexBuffer = nullptr;
        UINT VertexCount = 0;
        UINT IndexCount = 0;
        D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    };

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
    void ReleaseMeshResources();

    void PreparePipeline();
    void UpdateVSConstants(const FMatrix& World, const FMatrix& View, const FMatrix& Projection, const FVector4& Color);

    bool GetOrCreateMeshResource(UStaticMesh* Mesh, FMeshGPUResource& OutResource);
    D3D11_PRIMITIVE_TOPOLOGY ConvertTopology(EMeshTopology Topology) const;
    void DrawMeshItem(const FRenderItem& Item, const FMatrix& View, const FMatrix& Projection);

private:
    std::unordered_map<UStaticMesh*, FMeshGPUResource> MeshResourceMap;
};