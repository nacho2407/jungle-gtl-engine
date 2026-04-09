#include "StaticMesh.h"

#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(UStaticMesh, UObject)

UStaticMesh::~UStaticMesh()
{
    for (int32 i = 0; i < MAX_LOD; ++i)
    {
        if (LODMeshData[i] != nullptr && LODMeshData[i] != MeshData)
        {
            delete LODMeshData[i];
            LODMeshData[i] = nullptr;
        }
    }

    if (MeshData != nullptr)
    {
        delete MeshData;
        MeshData = nullptr;
    }
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
	RebuildLocalBoundsFromMeshData();
}

FStaticMesh* UStaticMesh::GetMeshData(int32 LOD)
{
	if (LOD <= 0)
        return MeshData;

    if (LOD >= ValidLODCount)
        LOD = ValidLODCount - 1;

    if (LOD <= 0)
        return MeshData;

    return LODMeshData[LOD];
}

const FStaticMesh* UStaticMesh::GetMeshData(int32 LOD) const
{	if (LOD <= 0)
        return MeshData;

    if (LOD >= ValidLODCount)
        LOD = ValidLODCount - 1;

    if (LOD <= 0)
        return MeshData;

    return LODMeshData[LOD];
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
//	준혁님이 수정 예정
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

void UStaticMesh::RebuildLocalBoundsFromMeshData()
{
	if (!MeshData)
	{
		return;
	}

	MeshData->LocalBounds.Reset();
	for (const FNormalVertex& Vertex : MeshData->Vertices)
	{
		MeshData->LocalBounds.Expand(Vertex.Position);
	}
}
