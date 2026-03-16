#pragma once

# include <vector>
#include <cstdint>

#include "UObject.h"
#include "Structs.h"
#include "Resource/MeshTypes.h"

class UStaticMesh : public UObject
{
public:
	UStaticMesh();
	virtual ~UStaticMesh();

public:
	void SetVertices(const std::vector<FVertexSimple>& InVertices); // @@@@ 이렇게 하면 VertexSimple 모양 데이터만 가능한 거 아님? 
	const std::vector<FVertexSimple>& GetVertices() const;

	void SetIndices(const std::vector<uint32_t>& InIndices);
	const std::vector<uint32_t>& GetIndices() const;

	void SetTopology(EMeshTopology InTopology);
	EMeshTopology GetTopology() const;

	uint32_t GetVertexCount() const;
	uint32_t GetIndexCount() const;
	bool HasIndices() const; 

private:
	std::vector<FVertexSimple> Vertices;
	std::vector<uint32_t> Indices;
	EMeshTopology Topology = EMeshTopology::TriangleList;
};