#include "RenderCommand.h"
#include "Material.h"
#include "MeshData.h"

uint64 FRenderCommand::MakeSortKey(const FMaterial* InMaterial, const FRenderMesh* InMeshData)
{
	const uint64 MatId = InMaterial ? (InMaterial->GetSortId() & 0xFFFFFFFFull) : 0;
	const uint64 MeshId = InMeshData ? static_cast<uint64>(InMeshData->GetSortId()) : 0;
	return (MatId << 32) | MeshId;
}
