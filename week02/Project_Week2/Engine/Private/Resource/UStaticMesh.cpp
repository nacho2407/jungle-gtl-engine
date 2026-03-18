#include "Resource/UStaticMesh.h"

UStaticMesh::UStaticMesh(){}
UStaticMesh::~UStaticMesh(){}

void UStaticMesh::SetVertices(const TArray<FVertexSimple>& InVertices)
{
	Vertices = InVertices;
}

const TArray<FVertexSimple>& UStaticMesh::GetVertices() const
{
	return Vertices;
}

void UStaticMesh::SetIndices(const TArray<uint32_t>& InIndices)
{
    Indices = InIndices;
}

const TArray<uint32_t>& UStaticMesh::GetIndices() const
{
    return Indices;
}

void UStaticMesh::SetTopology(EMeshTopology InTopology)
{
    Topology = InTopology;
}

EMeshTopology UStaticMesh::GetTopology() const
{
    return Topology;
}

uint32_t UStaticMesh::GetVertexCount() const
{
    return static_cast<uint32_t>(Vertices.size());
}

uint32_t UStaticMesh::GetIndexCount() const
{
    return static_cast<uint32_t>(Indices.size());
}

bool UStaticMesh::HasIndices() const
{
    return !Indices.empty();
}