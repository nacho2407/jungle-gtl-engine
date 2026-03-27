#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UStaticMesh, UObject)
REGISTER_FACTORY(UStaticMesh)

static const FString EmptyPath;

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
