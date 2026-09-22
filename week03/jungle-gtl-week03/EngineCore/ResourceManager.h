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

#include "Text/FontAsset.h"

class URenderer;

struct MeshData
{
	TArray<FVertexSimple> Vertices;
	TArray<uint32_t> Indices;
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
	FGeometry* CreateStaticGeometry(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies, void* Indices = nullptr, UINT IndicesByteWidth = 0)
	{
		auto iter = GeometryMap.find(ResourceName);
		if (iter != GeometryMap.end())
		{
			return nullptr;
		}

		FGeometry& Geometry = GeometryMap[ResourceName];

		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, Geometry.VertexBuffer.GetAddressOf());

		if (Indices != nullptr && IndicesByteWidth > 0)
		{
			D3D11_BUFFER_DESC ibDesc = {};
			ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
			ibDesc.ByteWidth = IndicesByteWidth;
			ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA ibData = { Indices };
			Device->CreateBuffer(&ibDesc, &ibData, Geometry.IndexBuffer.GetAddressOf());

			Geometry.IndexCount = IndicesByteWidth / sizeof(uint32_t);
			Geometry.IndexFormat = DXGI_FORMAT_R32_UINT;

			// Store CPU side indices for picking
			Geometry.Indices.assign((uint32_t*)Indices, (uint32_t*)Indices + Geometry.IndexCount);
		}

		Geometry.VertexStride = sizeof(T);
		Geometry.VertexCount = byteWidth / sizeof(T);
		Geometry.Offset = offset;
		Geometry.Vertices = Verticies;
		return &Geometry;
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

	FMaterial& CreateMaterial(WFString VertexShaderName, WFString PixelShaderName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC* layout, UINT layoutCount);
	TComPtr<ID3D11ShaderResourceView> LoadDDSTexture(WFString ResourceName, WFString FilePath);

	void AddCubeMesh(ID3D11Device* Device);
	void AddSphereMesh(ID3D11Device* Device);
	void AddTriangleMesh(ID3D11Device* Device);
	void AddPlaneMesh(ID3D11Device* Device);
	void AddBillboardQuadMesh(ID3D11Device* Device);
	void AddAxisMesh(ID3D11Device* Device);

	MeshData CreateCubeMeshData();

	MeshData CreateCircleMeshData(int sliceCount = 20, float radius = 0.5f, float height = 0.0f);

	MeshData CreateTorusMeshData(float majorRadius = 1.0f, float minorRadius = 0.1f, int radialSegments = 32, int tubularSegments = 16);

	MeshData CreateCylinderMeshData(int sliceCount = 20, float radius = 0.5f, float height = 1.0f);

	MeshData CreateConeMeshData(int sliceCount = 20, float radius = 0.5f, float height = 1.0f);

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

	FFontAsset& GetBillboardFont() { return BillboardFont; }

	/**
	 * @brief 문자열에 필요한 glyph를 billboard atlas에 보장합니다.
	 * 
	 * @param Text Glyph를 보장할 문자열
	 * 
	 * @return Atlas내 필요한 glyph 보장 여부
	 */
	bool EnsureBillboardGlyphs(const FString& Text);

private:
	ResourceManager() {}

	/**
	 * @brief GPU에 Atlas 텍스처를 최초 생성합니다.
	 * 
	 * @return Atlas 텍스처 생성 성공 여부
	 */
	bool CreateBillboardFontAtlasTexture(void);

	/**
	 * @brief 새로운 Glyph를 추가하기 위해 GPU 내 Atlas 텍스처를 업데이트합니다.
	 * 
	 * @param 업데이트할 Atlas 내 직사각형 부분 정보
	 * 
	 * @return Atlas 텍스처 업데이트 성공 여부
	 */
	bool UpdateBillboardFontAtlasRegion(const FAtlasRect& DirtyRect);

private://:ShaderW0.hlsl == ShaderLine.hlsl
	TMap<FString, FGeometry> GeometryMap;
	TMap<WFString, FMaterial> MaterialMap;
	TMap<WFString, TComPtr<ID3D11ShaderResourceView>> TextureMap;
	URenderer* Renderer = nullptr;

	/**
	 * @brief Billboard 텍스트 용 폰트 에셋
	 */
	FFontAsset BillboardFont;

	/**
	 * @brief 폰트 텍스처 정보. 원래는 SRV 생성 후 삭제하지만, UpdateSubresource를 통해 Atlas 부분 업데이트를 수행하기 위해 필요
	 */
	TComPtr<ID3D11Texture2D> BillboardFontTexture;
};