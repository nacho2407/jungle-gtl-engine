#include "PrimitiveLineBatch.h"

CPrimitiveLineBatch::CPrimitiveLineBatch()
{
	MeshData = std::make_shared<FMeshData>();
	MeshData->Topology = EMeshTopology::EMT_LineList;
}

uint32 CPrimitiveLineBatch::AddLine(FVector InStart, FVector InEnd, FVector4 InColor, uint32 InBatchID)
{
	uint32 meshDataSize = MeshData->Vertices.size();
	const FVector normal = FVector::ZeroVector;
	MeshData->Vertices.push_back({ InStart, InColor,  normal });
	MeshData->Vertices.push_back({ InEnd, InColor, normal });
	MeshData->Indices.push_back(meshDataSize);
	MeshData->Indices.push_back(meshDataSize + 1);

	MeshData->bIsDirty = true;
	MeshData->UpdateLocalBound();

	// TODO: batchID 구현 (추가한 batchLine 수동으로 삭제하는 용도)
	return InBatchID;
}

void CPrimitiveLineBatch::ClearVertices()
{
	MeshData->Vertices.clear();
	MeshData->Indices.clear();
	MeshData->bIsDirty = true;
	MeshData->UpdateLocalBound();
}