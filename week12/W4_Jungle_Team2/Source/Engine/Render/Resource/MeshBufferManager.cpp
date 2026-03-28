#include "MeshBufferManager.h"

#include "Asset/StaticMesh.h"

namespace
{
	FMeshData CreateBillboardQuadMeshData()
	{
		FMeshData QuadMeshData;
		FColor DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f,  0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f,  0.5f, -0.5f), DefaultColor, 0 });
		QuadMeshData.Vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), DefaultColor, 0 });

		QuadMeshData.Indices = { 0, 1, 2, 0, 2, 3 };
		return QuadMeshData;
	}

	FMeshData ToMeshData(const UStaticMesh* StaticMeshAsset)
	{
		FMeshData Result;
		if (!StaticMeshAsset || !StaticMeshAsset->HasValidMeshData())
		{
			return Result;
		}

		const TArray<FNormalVertex>& SourceVertices = StaticMeshAsset->GetVertices();
		Result.Vertices.reserve(SourceVertices.size());

		for (const FNormalVertex& Vertex : SourceVertices)
		{
			FVertex PrimitiveVertex = {};
			PrimitiveVertex.Position = Vertex.Position;
			PrimitiveVertex.Color = Vertex.Color;
			PrimitiveVertex.SubID = 0;
			Result.Vertices.push_back(PrimitiveVertex);
		}

		Result.Indices = StaticMeshAsset->GetIndices();
		return Result;
	}
}

void FMeshBufferManager::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
	const FMeshData QuadMeshData = CreateBillboardQuadMeshData();

	MeshBufferMap[EPrimitiveType::EPT_TransGizmo].Create(InDevice, FEditorMeshLibrary::GetTranslationGizmo());
	MeshBufferMap[EPrimitiveType::EPT_RotGizmo].Create(InDevice, FEditorMeshLibrary::GetRotationGizmo()); 
	MeshBufferMap[EPrimitiveType::EPT_ScaleGizmo].Create(InDevice, FEditorMeshLibrary::GetScaleGizmo());
	MeshBufferMap[EPrimitiveType::EPT_Billboard].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_SubUV].Create(InDevice, QuadMeshData);
	MeshBufferMap[EPrimitiveType::EPT_Text].Create(InDevice, QuadMeshData);
}

//	TODO : 내일 하기
void FMeshBufferManager::Release()
{
	for (auto& pair : MeshBufferMap)
	{
		pair.second.Release();
	}
	MeshBufferMap.clear();

	for (auto& pair : StaticMeshBufferMap)
	{
		pair.second.Release();
	}
	StaticMeshBufferMap.clear();
	Device = nullptr;
}

//	MeshBuffer는 VB, IB를 모두 포함하고 있습니다.
FMeshBuffer& FMeshBufferManager::GetMeshBuffer(EPrimitiveType InPrimitiveType)
{
	auto it = MeshBufferMap.find(InPrimitiveType);
	if (it != MeshBufferMap.end())
	{
		return it->second;
	}
	
	//	존재하지 않는 PrimitiveType이 요청된 경우, Billboard Quad를 기본 반환합니다.
	return MeshBufferMap.at(EPrimitiveType::EPT_Billboard);
}

FMeshBuffer* FMeshBufferManager::GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset)
{
	if (!Device || !StaticMeshAsset || !StaticMeshAsset->HasValidMeshData())
	{
		return nullptr;
	}

	auto It = StaticMeshBufferMap.find(StaticMeshAsset);
	if (It != StaticMeshBufferMap.end())
	{
		return &It->second;
	}

	FMeshData PrimitiveMeshData = ToMeshData(StaticMeshAsset);
	if (PrimitiveMeshData.Vertices.empty() || PrimitiveMeshData.Indices.empty())
	{
		return nullptr;
	}

	FMeshBuffer& NewBuffer = StaticMeshBufferMap[StaticMeshAsset];
	NewBuffer.Create(Device, PrimitiveMeshData);
	return &NewBuffer;
}