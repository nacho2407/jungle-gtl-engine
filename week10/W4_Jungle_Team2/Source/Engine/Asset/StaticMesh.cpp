#include "StaticMesh.h"

#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(UStaticMesh, UObject)

UStaticMesh::~UStaticMesh()
{
	delete MeshData;
	MeshData = nullptr;
}

void UStaticMesh::SetMeshData(FStaticMesh* InMeshData)
{
	if (MeshData == InMeshData)
	{
		return;
	}

	delete MeshData;
	MeshData = InMeshData;
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
	//	UE_LOG로 남길까 했지만 의미가 없음
	static_assert(MeshData != nullptr, "MeshData is nullptr");

	return MeshData->Vertices;
}

const TArray<uint32>& UStaticMesh::GetIndices() const
{
	static_assert(MeshData != nullptr, "MeshData is nullptr");

	return MeshData->Indices;
}

const TArray<FStaticMeshSection>& UStaticMesh::GetSections() const
{
	static_assert(MeshData != nullptr, "MeshData is nullptr");

	return MeshData->Sections;
}

const TArray<FStaticMeshMaterialSlot>& UStaticMesh::GetMaterialSlots() const
{
	static_assert(MeshData != nullptr, "MeshData is nullptr");

	return MeshData->MaterialSlots;
}

const FAABB& UStaticMesh::GetLocalBounds() const
{
	static_assert(MeshData != nullptr, "MeshData is nullptr");

	return MeshData->LocalBounds;
}

bool UStaticMesh::HasValidMeshData() const
{
	return MeshData != nullptr
		&& !MeshData->Vertices.empty()
		&& !MeshData->Indices.empty();
}
