#include "SkeletalMesh.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

static const FString EmptyPath;

void USkeletalMesh::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() && !SkeletalMeshAsset)
	{
		SkeletalMeshAsset = new FSkeletalMesh();
	}

	Ar << SkeletalMeshAsset->Vertices;
	Ar << SkeletalMeshAsset->Indices;
}

const FString& USkeletalMesh::GetAssetPathFileName() const
{
	if (SkeletalMeshAsset)
	{
		return SkeletalMeshAsset->PathFileName;
	}
	return EmptyPath;
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	SkeletalMeshAsset = InMesh;
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
	return SkeletalMeshAsset;
}

void USkeletalMesh::SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials)
{
	SkeletalMaterials = InMaterials;
}

const TArray<FSkeletalMaterial>& USkeletalMesh::GetSkeletalMaterials() const
{
	return SkeletalMaterials;
}

void USkeletalMesh::InitResources(ID3D11Device* InDevice)
{
	if (!InDevice || !SkeletalMeshAsset) return;

	const uint32 CPUSize =
		static_cast<uint32>(SkeletalMeshAsset->Vertices.size() * sizeof(FVertexPNCTBW)) +
		static_cast<uint32>(SkeletalMeshAsset->Indices.size() * sizeof(uint32));
	MemoryStats::AddSkeletalMeshCPUMemory(CPUSize);

	TMeshData<FVertexPNCTBW> RenderMeshData;
	RenderMeshData.Vertices.reserve(SkeletalMeshAsset->Vertices.size());

	for (const FVertexPNCTBW& RawVert : SkeletalMeshAsset->Vertices)
	{
		FVertexPNCTBW RenderVert;
		RenderVert.Position = RawVert.Position;
		RenderVert.Normal = RawVert.Normal;
		RenderVert.Color = RawVert.Color;
		RenderVert.UV = RawVert.UV;
		std::copy(std::begin(RawVert.BoneIndices), std::end(RawVert.BoneIndices), std::begin(RenderVert.BoneIndices));
		std::copy(std::begin(RawVert.BoneWeights), std::end(RawVert.BoneWeights), std::begin(RenderVert.BoneWeights));
		RenderMeshData.Vertices.push_back(RenderVert);
	}
	RenderMeshData.Indices = SkeletalMeshAsset->Indices;

	SkeletalMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
	SkeletalMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);
}
