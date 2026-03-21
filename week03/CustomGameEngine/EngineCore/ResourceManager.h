#pragma once

#include "EngineCore/EngineTypes.h"
#include "Renderer/Geometry.h"
#include "Renderer/Material.h"
#include "Math/MathHelper.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "VertexSimple.h"
#include "EngineCore/Name.h"
#include <d3d11.h>
#include <d3dcompiler.h>

class URenderer;

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
	void CreateStaticGeometry(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		auto iter = GeometryMap.find(ResourceName);
		if (iter != GeometryMap.end())
		{
			return;
		}
		FGeometry& Geometry = GeometryMap[ResourceName];


		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, Geometry.VertexBuffer.GetAddressOf());

		Geometry.VertexStride = sizeof(T);
		Geometry.VertexCount = byteWidth / sizeof(T);
		Geometry.Offset = offset;
		Geometry.Vertices = Verticies;
	}
	void CreateNoBufferGeometry(FString ResourceName, const TArray<FVector>& Verticies);

	template <typename T>
	void UpdateStaticGeomtry(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		bool bWasNull = true;
		auto iter = GeometryMap.find(ResourceName);
		if (iter == GeometryMap.end())
		{
			return;
		}
		else
		{
			FGeometry& Geometry = iter->second;
			if (Geometry.VertexBuffer != nullptr)
			{
				Geometry.VertexBuffer.Reset();
				bWasNull = false;
			}
		}
		FGeometry& Geometry = GeometryMap[ResourceName];


		if (!bWasNull)
		{
			D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
			if (Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, Geometry.VertexBuffer.GetAddressOf()));
		}

		Geometry.VertexStride = sizeof(T);
		Geometry.VertexCount = byteWidth / sizeof(T);
		Geometry.Offset = offset;
		Geometry.Vertices = Verticies;
	}

	void CreateMaterial(WFString VertexShaderName, WFString PixelShaderName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC* layout, UINT layoutCount);

	void AddCubeMesh(ID3D11Device* Device);
	void AddSphereMesh(ID3D11Device* Device);
	void AddTriangleMesh(ID3D11Device* Device);
	void AddPlaneMesh(ID3D11Device* Device);
	void AddAxisMesh(ID3D11Device* Device);

	TArray<FVertexSimple> CreateCubeVertices();

	TArray<FVertexSimple> CreateCircleVertices(int sliceCount = 20, float radius = 0.5f, float height = 0.0f);

	TArray<FVertexSimple> CreateTorusVertices(float majorRadius = 1.0f, float minorRadius = 0.1f, int radialSegments = 32, int tubularSegments = 16);

	TArray<FVertexSimple> CreateCylinderVertices(int sliceCount = 20, float radius = 0.5f, float height = 1.0f);

	TArray<FVertexSimple> CreateConeVertices(int sliceCount = 20, float radius = 0.5f, float height = 1.0f);

	void AddGizmoTranslationMesh(ID3D11Device* Device);

	void AddGizmoRotationMesh(ID3D11Device* Device);

	void AddGizmoScaleMesh(ID3D11Device* Device);

	TArray<FVertexSimple> MakeGridMesh(ID3D11Device* Device, float halfExtent, float cellSize, float z = 0.f);

	void AddGridMesh(ID3D11Device* Device, float halfExtent, float cellSize, float z = 0.f);
	void UpdateGridMesh(float halfExtent, float cellSize, float z = 0.f);

	FMaterial& GetMaterial(WFString ResourceName);
	FGeometry& GetGeometry(FString ResourceName);
	void Release();

	//void InjectRenderer(URenderer* InRenderer) { Renderer = InRenderer; }
	void Initialize(URenderer* InRenderer);
private:
	ResourceManager() {}
	TUMap<FString, FGeometry> GeometryMap;
	TUMap<WFString, FMaterial> MaterialMap;
	URenderer* Renderer = nullptr;
};