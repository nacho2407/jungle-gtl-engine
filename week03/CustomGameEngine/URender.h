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

class URenderer {
public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;

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

	void Release();

	void SwapBuffer();

public:
	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	void CreateShader();

	void ReleaseShader();

	void Prepare();
	void PrepareShader();

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);

public:
	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

public:
	struct FConstantBufferData {
		FMatrix Model;
		FMatrix Veiw;
		FMatrix Projection;
	};
	ID3D11Buffer* ConstantBuffer{ nullptr };

	void CreateConstantBuffer();

	void ReleaseConstantBuffer();

	void UpdateConstant(const FConstantBufferData& Data);


public:
	struct FMeshData
	{
		ID3D11Buffer* VertexBuffer;
		ID3D11Buffer* IndexBuffer;
		ID3D11InputLayout* Layout;
		ID3D11VertexShader* VS;
		ID3D11PixelShader* PS;
		int VertexCount = 0;
	};

	struct FRenderData
	{
		FMatrix Scale;
		FMatrix Rotation;
		FMatrix Translation;
		FMeshData MeshData;
	};

	void DrawPrimitive(FVector Scale, FVector Rotation, FVector Translation, std::string MeshID);
	void AddCubeMesh();
	void AddSphereMesh();
	void Render();


	std::vector<FRenderData> RenderDataContainer;
	std::unordered_map<std::string, FMeshData> ResourceContainer;

};
