#include "StaticMesh.h"

#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(UStaticMesh, UObject)

UStaticMesh::~UStaticMesh()
{
	delete MeshData;
	MeshData = nullptr;
}

void UStaticMesh::SetMeshData(FStaticMesh* InMeshData, const TArray<FStaticMeshMaterialSlot>& MaterialSlot)
{
	if (MeshData == InMeshData)
	{
		return;
	}

	delete MeshData;
	MeshData = InMeshData;
	MaterialSlots = MaterialSlot;
}

FStaticMesh* UStaticMesh::GetMeshData()
{
	return MeshData;
}

const FStaticMesh* UStaticMesh::GetMeshData() const
{
	return MeshData;
}

const FString& UStaticMesh::GetAssetPathFileName() const
{
	static FString empty = {};
	return MeshData ? MeshData->PathFileName : empty;
}

const TArray<FNormalVertex>& UStaticMesh::GetVertices() const
{
	static const TArray<FNormalVertex> Empty = {};
	return MeshData ? MeshData->Vertices : Empty;
}

const TArray<uint32>& UStaticMesh::GetIndices() const
{
	static const TArray<uint32> Empty = {};
	return MeshData ? MeshData->Indices : Empty;
}

const TArray<FStaticMeshSection>& UStaticMesh::GetSections() const
{
	static const TArray<FStaticMeshSection> Empty = {};
	return MeshData ? MeshData->Sections : Empty;
}

const TArray<FStaticMeshMaterialSlot>& UStaticMesh::GetMaterialSlots() const
{
	static const TArray<FStaticMeshMaterialSlot> Empty = {};
	return MeshData ? MaterialSlots : Empty;
}

const FAABB& UStaticMesh::GetLocalBounds() const
{
	static const FAABB Empty = {};
	return MeshData ? MeshData->LocalBounds : Empty;
}

bool UStaticMesh::HasValidMeshData() const
{
	return MeshData != nullptr
		&& !MeshData->Vertices.empty()
		&& !MeshData->Indices.empty();
}
