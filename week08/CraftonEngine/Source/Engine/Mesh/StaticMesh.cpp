#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Serialization/WindowsArchive.h"
#include "Mesh/ObjImporter.h"
#include "Texture/Texture2D.h"
#include "Engine/Profiling/MemoryStats.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)

static const FString EmptyPath;

UStaticMesh::~UStaticMesh()
{
	if (StaticMeshAsset)
	{
		const uint32 CPUSize =
			static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FNormalVertex)) +
			static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));

		MemoryStats::SubStaticMeshCPUMemory(CPUSize);
	}
}

void UStaticMesh::Serialize(FArchive& Ar)
{
	// 에셋이 비어있으면 로드용으로 생성
	if (Ar.IsLoading() && !StaticMeshAsset)
	{
		StaticMeshAsset = new FStaticMesh();
	}

	// 1. 지오메트리 데이터 직렬화
	StaticMeshAsset->Serialize(Ar);

	// 2. 머티리얼 데이터 직렬화 (필수!)
	Ar << StaticMaterials;
}

void UStaticMesh::InitResources(ID3D11Device* InDevice)
{
	if (!InDevice || !StaticMeshAsset) return;

	// CPU 메모리 추적
	const uint32 CPUSize =
		static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FNormalVertex)) +
		static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));
	MemoryStats::AddStaticMeshCPUMemory(CPUSize);

	// CPU → GPU 정점 버퍼 변환
	TMeshData<FVertexPNCT> RenderMeshData;
	RenderMeshData.Vertices.reserve(StaticMeshAsset->Vertices.size());

	for (const FNormalVertex& RawVert : StaticMeshAsset->Vertices)
	{
		FVertexPNCT RenderVert;
		RenderVert.Position = RawVert.pos;
		RenderVert.Normal = RawVert.normal;
		RenderVert.Color = RawVert.color;
		RenderVert.UV = RawVert.tex;
		RenderMeshData.Vertices.push_back(RenderVert);
	}
	RenderMeshData.Indices = StaticMeshAsset->Indices;

	StaticMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
	StaticMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);

	// 머티리얼 텍스처 프리로드
	for (auto& Mat : StaticMaterials)
	{
		if (Mat.MaterialInterface && !Mat.MaterialInterface->DiffuseTextureFilePath.empty())
		{
			Mat.MaterialInterface->DiffuseTexture = UTexture2D::LoadFromFile(
				Mat.MaterialInterface->DiffuseTextureFilePath, InDevice);
		}
	}
}

const FString& UStaticMesh::GetAssetPathFileName() const
{
	if (StaticMeshAsset)
	{
		return StaticMeshAsset->PathFileName;
	}
	return EmptyPath;
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InMesh)
{
	StaticMeshAsset = InMesh;
}

FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
	return StaticMeshAsset;
}

void UStaticMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
	StaticMaterials = InMaterials;
}

const TArray<FStaticMaterial>& UStaticMesh::GetStaticMaterials() const
{
	return StaticMaterials;
}
