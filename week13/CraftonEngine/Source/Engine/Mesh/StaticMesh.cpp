#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UStaticMesh, UObject)
REGISTER_FACTORY(UStaticMesh)

static const std::string EmptyPath;

const std::string& UStaticMesh::GetAssetPathFileName() const
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
