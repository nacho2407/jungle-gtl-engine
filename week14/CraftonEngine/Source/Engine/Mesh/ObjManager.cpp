#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Render/Types/VertexTypes.h"
#include "ObjImporter.h"
#include "Serialization/WindowsArchive.h"

std::map<std::string, UStaticMesh*> FObjManager::StaticMeshCache;

std::string FObjManager::GetBinaryFilePath(const std::string& OriginalPath)
{
	size_t DotPos = OriginalPath.find_last_of('.');
	if (DotPos != std::string::npos)
	{
		return OriginalPath.substr(0, DotPos) + ".bin";
	}
	return OriginalPath + ".bin";
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const std::string& PathFileName, ID3D11Device* InDevice)
{
	// 캐시 확인 (O(1) 룩업)
	auto It = StaticMeshCache.find(PathFileName);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// 파싱 + GPU 버퍼 생성
	std::unique_ptr<FStaticMesh> Asset = LoadStaticMeshAsset(PathFileName, InDevice);
	if (!Asset)
	{
		return nullptr;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	StaticMesh->SetStaticMeshAsset(std::move(Asset));

	// 캐시 등록
	StaticMeshCache[PathFileName] = StaticMesh;

	return StaticMesh;
}

std::unique_ptr<FStaticMesh> FObjManager::LoadStaticMeshAsset(const std::string& PathFileName, ID3D11Device* InDevice)
{
	auto Result = std::make_unique<FStaticMesh>();

	std::string BinPath = GetBinaryFilePath(PathFileName);
	FWindowsBinReader Reader(BinPath);
	if (Reader.IsValid())
	{
		Result->Serialize(Reader);
	}
	else
	{
		// 바이너리 파일이 없음 -> OBJ 파싱 및 굽기 (최초 1회)
		FStaticMesh ConvertedMesh;
		TArray<FStaticMaterial> ImportedMaterials;
		if (!FObjImporter::Import(PathFileName, ConvertedMesh, ImportedMaterials))
		{
			return nullptr;
		}

		FWindowsBinWriter Writer(BinPath);
		if (Writer.IsValid())
		{
			ConvertedMesh.Serialize(Writer);
		}

		*Result = std::move(ConvertedMesh);
	}

	Result->PathFileName = PathFileName;

	// CPU → GPU 버퍼 변환
	TMeshData<FVertexPNCT> RenderMeshData;
	RenderMeshData.Vertices.reserve(Result->Vertices.size());

	for (const FNormalVertex& RawVert : Result->Vertices)
	{
		FVertexPNCT RenderVert;
		RenderVert.Position = RawVert.pos;
		RenderVert.Normal = RawVert.normal;
		RenderVert.Color = RawVert.color;
		RenderVert.UV = RawVert.tex;
		RenderMeshData.Vertices.push_back(RenderVert);
	}
	RenderMeshData.Indices = Result->Indices;

	Result->RenderBuffer = std::make_unique<FMeshBuffer>();
	if (InDevice)
	{
		Result->RenderBuffer->Create(InDevice, RenderMeshData);
	}

	return Result;
}
