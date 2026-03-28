#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)

static const FString EmptyPath;

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

const TArray<FStaticMaterial>& UStaticMesh::GetStaticMaterials() const
{
	return StaticMaterials;
}
