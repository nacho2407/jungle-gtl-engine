#include "Mesh/ObjManager.h"
#include "Mesh/StaticMeshAsset.h"
#include "Mesh/StaticMesh.h"
#include "Render/Resource/VertexTypes.h"
#include "ObjImporter.h"

std::map<std::string, FStaticMesh> FObjManager::ObjStaticMeshMap;
ID3D11Device* FObjManager::GDevice = nullptr;

FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const std::string& PathFileName)
{
	// 1. std::map의 검색 함수는 소문자 'find'이며, 반복자(iterator)를 반환합니다.
	auto It = ObjStaticMeshMap.find(PathFileName);

	// 2. 검색에 성공했다면 (end()가 아니라면)
	if (It != ObjStaticMeshMap.end())
	{
		// 3. It->first는 키(string), It->second가 값(FStaticMesh*)입니다.
		return &It->second;
	}

	// 1. 파싱
	FObjInfo ParsedObjInfo = FObjImporter::ParseObj(PathFileName);

	// 2. 변환된 임시 객체를 Map 안에 통째로 집어넣음 (이동)
	ObjStaticMeshMap[PathFileName] = FObjImporter::Convert(ParsedObjInfo);

	// 3. Map 안에 안전하게 정착한 진짜 객체의 참조를 가져옴
	FStaticMesh& ConvertedMesh = ObjStaticMeshMap[PathFileName];

	// 4. 경로 이름 세팅
	ConvertedMesh.PathFileName = PathFileName;

	TArray<FVertexPNCT> RenderVertices;
	RenderVertices.reserve(ConvertedMesh.Vertices.size());

	for (const FNormalVertex& RawVert : ConvertedMesh.Vertices)
	{
		FVertexPNCT RenderVert;
		RenderVert.Position = RawVert.pos;
		RenderVert.Normal = RawVert.normal;
		RenderVert.Color = RawVert.color;
		RenderVert.UV = RawVert.tex;
		RenderVertices.push_back(RenderVert);
	}

	// 2. MeshBuffer 객체 생성 및 GPU에 버퍼 굽기
	ConvertedMesh.RenderBuffer = new FMeshBuffer();

	if (GDevice)
	{
		uint32 VCount = static_cast<uint32>(RenderVertices.size());
		uint32 VByteWidth = VCount * sizeof(FVertexPNCT);
		ConvertedMesh.RenderBuffer->GetVertexBuffer().Create(
			GDevice, RenderVertices.data(), VCount, VByteWidth, sizeof(FVertexPNCT));

		if (!ConvertedMesh.Indices.empty())
		{
			uint32 ICount = static_cast<uint32>(ConvertedMesh.Indices.size());
			uint32 IByteWidth = ICount * sizeof(uint32);
			ConvertedMesh.RenderBuffer->GetIndexBuffer().Create(
				GDevice, ConvertedMesh.Indices.data(), ICount, IByteWidth);
		}
	}

	// 5. 안전한 메모리 주소 반환
	return &ConvertedMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const std::string& PathFileName)
{
	// 1. 기존 엔진 메모리에 이미 UStaticMesh 껍데기가 있는지 검사
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		UStaticMesh* StaticMesh = *It;
		if (StaticMesh && StaticMesh->GetAssetPathFileName() == PathFileName)
		{
			return *It;
		}
	}

	// 2. 엔진 메모리에 없다면, Raw 데이터(FStaticMesh)를 매니저에서 가져옵니다.
	FStaticMesh* Asset = FObjManager::LoadObjStaticMeshAsset(PathFileName);
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	StaticMesh->SetStaticMeshAsset(Asset);

	return StaticMesh;
}
