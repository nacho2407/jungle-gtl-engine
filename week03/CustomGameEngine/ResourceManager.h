#pragma once
#include "UEngineTypes.h"
#include "d3d11.h"
#include "d3dcompiler.h"
#include "FVertexSimple.h"
#include "FVector.h"
#include "FMaterial.h"
#include "FGeometry.h"
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
	static void CreateGeometry(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		ID3D11Buffer* vertexBuffer;
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		FGeometry& Geometry = GeometryMap[ResourceName];
		Geometry.VertexBuffer = vertexBuffer;
		Geometry.VertexStride = sizeof(T);
		Geometry.VertexCount = byteWidth / sizeof(T);
		Geometry.Offset = offset;
		Geometry.Vertices = Verticies;
	}

	static void CreateMaterial(WFString VertexShaderName, WFString PixelShaderName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC layout[], UINT layoutCount)
	{
		ID3D11VertexShader* VertexShader = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3DBlob* vertexshaderCSO = nullptr;
		D3DCompileFromFile(VertexShaderName.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0,
			0, &vertexshaderCSO, nullptr);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), nullptr,
			&VertexShader);
		Device->CreateInputLayout(
			layout, layoutCount, vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), &InputLayout);

		ID3D11PixelShader* PixelShader = nullptr;
		ID3DBlob* pixelshaderCSO = nullptr;
		D3DCompileFromFile(PixelShaderName.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0,
			0, &pixelshaderCSO, nullptr);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
			pixelshaderCSO->GetBufferSize(), nullptr,
			&PixelShader);

		FMaterial& Mat = MaterialMap[VertexShaderName];
		Mat.VertexShader = VertexShader;
		Mat.PixelShader = PixelShader;
		Mat.InputLayout = InputLayout;

		pixelshaderCSO->Release();
		vertexshaderCSO->Release();
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

		int Cnt = sizeof(cubeVertices) / sizeof(FVertexSimple);
		TArray<FVector> Vertices(Cnt);
		for (int i = 0;i < Cnt;++i)
		{
			Vertices[i] = FVector(cubeVertices[i].x, cubeVertices[i].y, cubeVertices[i].z);
		}
		CreateGeometry("Cube", cubeVertices, sizeof(cubeVertices), Device, 0, Vertices);
	}

	static FGeometry& GetGeometry(FString ResourceName)
	{
		return GeometryMap[ResourceName];
	}
	static FMaterial& GetMaterial(WFString ResourceName)
	{
		return MaterialMap[ResourceName];
	}
private:
	ResourceManager() {};
	static TMap<FString, FGeometry> GeometryMap;
	static TMap<WFString, FMaterial> MaterialMap;
};