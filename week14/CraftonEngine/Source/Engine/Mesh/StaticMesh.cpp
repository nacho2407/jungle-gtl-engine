#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Serialization/WindowsArchive.h"
#include "Mesh/ObjImporter.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)

static const FString EmptyPath;

void UStaticMesh::Build(const std::string& SourceFilePath, ID3D11Device* InDevice)
{
	std::string BinPath = FObjManager::GetBinaryFilePath(SourceFilePath);

	FWindowsBinReader Reader(BinPath);
	if (Reader.IsValid())
	{
		// 1. 캐시 히트: 무거운 .obj 파싱 없이 빠르게 바이너리 통째로 복사 (O(1) 수준)
		this->Serialize(Reader);
	}
	else
	{
		// 2. 캐시 미스: 원본 OBJ 파싱 진행
		auto NewMeshAsset = std::make_unique<FStaticMesh>();
		TArray<FStaticMaterial> ParsedMaterials;

		if (FObjImporter::Import(SourceFilePath, *NewMeshAsset, ParsedMaterials))
		{
			NewMeshAsset->PathFileName = SourceFilePath;
			this->SetStaticMeshAsset(std::move(NewMeshAsset));
			this->SetStaticMaterials(std::move(ParsedMaterials));

			// 3. 파싱 결과를 하드디스크에 굽기 (다음 로딩 속도 최적화)
			FWindowsBinWriter Writer(BinPath);
			if (Writer.IsValid())
			{
				this->Serialize(Writer);
			}
		}
	}

	// 4. 캐시에서 불렀든 파싱을 했든, 최종적으로 GPU 버퍼와 텍스처를 세팅합니다.
	InitResources(InDevice);
}

void UStaticMesh::Serialize(FArchive& Ar)
{
	// 에셋이 비어있으면 로드용으로 생성
	if (Ar.IsLoading() && !StaticMeshAsset)
	{
		StaticMeshAsset = std::make_unique<FStaticMesh>();
	}

	// 1. 지오메트리 데이터 직렬화
	StaticMeshAsset->Serialize(Ar);

	// 2. 머티리얼 데이터 직렬화 (필수!)
	Ar << StaticMaterials;
}

void UStaticMesh::InitResources(ID3D11Device* InDevice)
{
	if (!InDevice || !StaticMeshAsset) return;

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

void UStaticMesh::SetStaticMeshAsset(std::unique_ptr<FStaticMesh> InMesh)
{
	StaticMeshAsset = std::move(InMesh);
}

FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
	return StaticMeshAsset.get();
}

void UStaticMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
	StaticMaterials = std::move(InMaterials);
}

const TArray<FStaticMaterial>& UStaticMesh::GetStaticMaterials() const
{
	return StaticMaterials;
}
