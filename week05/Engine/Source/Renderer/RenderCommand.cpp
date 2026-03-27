#include "RenderCommand.h"
#include "Material.h"
#include "Primitive/PrimitiveBase.h"

uint64 FRenderCommand::MakeSortKey(const FMaterial* InMaterial, const FMeshData* InMeshData)
{
	uint32 MatId = InMaterial ? InMaterial->GetSortId() : 0;
	uint32 MeshId = InMeshData ? InMeshData->GetSortId() : 0;
	return (static_cast<uint64>(MatId) << 32) | MeshId;
}