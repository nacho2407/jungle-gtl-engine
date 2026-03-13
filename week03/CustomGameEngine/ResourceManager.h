#pragma once
#include "UEngineTypes.h"
#include "d3d11.h"
#include "d3dcompiler.h"
#include "FVertexSimple.h"
struct VertexResource
{
	ID3D11Buffer* VertexBuffer;
	int stride = 0;
	int vertexCount = 0;
	int offset = 0;
};
class ResourceManager
{
public:
	static ResourceManager* GetInstance()
	{
		static ResourceManager instance;
		return &instance;
	};

	~ResourceManager() {};

	template<typename T>
	static void CreateVertexResource(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		ID3D11Buffer* vertexBuffer;
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		VertexResource& Resource = VertexResourceMap[ResourceName];
		Resource.VertexBuffer = vertexBuffer;
		Resource.stride = sizeof(T);
		Resource.vertexCount = byteWidth / sizeof(T);
		Resource.offset = offset;
	}

	static void CompileVertexShaderAndLayout(WFString VertexShaderName, FString ShaderMainName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC layout[], UINT layoutCount)
	{
		ID3D11VertexShader* VertexShader = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3DBlob* vertexshaderCSO = nullptr;
		D3DCompileFromFile(VertexShaderName.c_str(), nullptr, nullptr, ShaderMainName.c_str(), "vs_5_0", 0,
			0, &vertexshaderCSO, nullptr);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), nullptr,
			&VertexShader);
		Device->CreateInputLayout(
			layout, layoutCount, vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), &InputLayout);

		VertexShaderMap[VertexShaderName] = VertexShader;
		InputLayoutMap[VertexShaderName] = InputLayout;

		vertexshaderCSO->Release();
	}

	static void CompilePixelShader(WFString PixelShaderName, FString ShaderMainName, ID3D11Device* Device)
	{
		ID3D11PixelShader* PixelShader = nullptr;
		ID3DBlob* pixelshaderCSO = nullptr;
		D3DCompileFromFile(PixelShaderName.c_str(), nullptr, nullptr, ShaderMainName.c_str(), "ps_5_0", 0,
			0, &pixelshaderCSO, nullptr);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
			pixelshaderCSO->GetBufferSize(), nullptr,
			&PixelShader);
		PixelShaderMap[PixelShaderName] = PixelShader;
		pixelshaderCSO->Release();
	}



	//Temporary Function
	static void AddCubeMesh(ID3D11Device* Device)
	{
		FVertexSimple cubeVertices[] =
		{
			// Front (+Z) - Red
			{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{  0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{  0.5f, 0.5f, 0.5f, 1,0,0,1 },

			{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{  0.5f, 0.5f, 0.5f, 1,0,0,1 },
			{ -0.5f, 0.5f, 0.5f, 1,0,0,1 },


			// Back (-Z) - Green
			{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },

			{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },
			{  0.5f, 0.5f,-0.5f, 0,1,0,1 },


			// Left (-X) - Blue
			{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
			{ -0.5f,-0.5f, 0.5f, 0,0,1,1 },
			{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },

			{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
			{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },
			{ -0.5f, 0.5f,-0.5f, 0,0,1,1 },


			// Right (+X) - Yellow
			{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
			{ 0.5f,-0.5f,-0.5f, 1,1,0,1 },
			{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },

			{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
			{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },
			{ 0.5f, 0.5f, 0.5f, 1,1,0,1 },


			// Top (+Y) - Cyan
			{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
			{  0.5f,0.5f, 0.5f, 0,1,1,1 },
			{  0.5f,0.5f,-0.5f, 0,1,1,1 },

			{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
			{  0.5f,0.5f,-0.5f, 0,1,1,1 },
			{ -0.5f,0.5f,-0.5f, 0,1,1,1 },


			// Bottom (-Y) - Magenta
			{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{  0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{  0.5f,-0.5f, 0.5f, 1,0,1,1 },

			{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{  0.5f,-0.5f, 0.5f, 1,0,1,1 },
			{ -0.5f,-0.5f, 0.5f, 1,0,1,1 },
		};

		CreateVertexResource("Cube", cubeVertices, sizeof(cubeVertices), Device, 0);
	}

	static VertexResource GetVertexResource(FString ResourceName)
	{
		return VertexResourceMap[ResourceName];
	}
	static ID3D11InputLayout* GetInputLayout(WFString VertexShaderName)
	{
		return InputLayoutMap[VertexShaderName];
	}
	static ID3D11VertexShader* GetVertexShader(WFString VertexShaderName)
	{
		return VertexShaderMap[VertexShaderName];
	}
	static ID3D11PixelShader* GetPixelShader(WFString PixelShaderName)
	{
		return PixelShaderMap[PixelShaderName];
	}
private:
	ResourceManager() {};
	static TMap<FString, VertexResource> VertexResourceMap;
	static TMap<WFString, ID3D11InputLayout*> InputLayoutMap;
	static TMap<WFString, ID3D11PixelShader*> PixelShaderMap;
	static TMap<WFString, ID3D11VertexShader*> VertexShaderMap;
};