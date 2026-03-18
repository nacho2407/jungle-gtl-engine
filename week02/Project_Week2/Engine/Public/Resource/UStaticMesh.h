#pragma once

# include <vector>
#include <cstdint>

#include "UObject.h"
#include "Structs.h"
#include "Resource/MeshTypes.h"

class UStaticMesh : public UObject
{
public:
	DECLARE_UClass(UStaticMesh, UObject)

	UStaticMesh();
	virtual ~UStaticMesh();

public:
	void SetVertices(const TArray<FVertexSimple>& InVertices); // @@@@ 이렇게 하면 VertexSimple 모양 데이터만 가능한 거 아님? 
	const TArray<FVertexSimple>& GetVertices() const;

	void SetIndices(const TArray<uint32>& InIndices);
	const TArray<uint32>& GetIndices() const;

	void SetTopology(EMeshTopology InTopology);
	EMeshTopology GetTopology() const;

	uint32 GetVertexCount() const;
	uint32 GetIndexCount() const;
	bool HasIndices() const; 

private:
	TArray<FVertexSimple> Vertices;
	TArray<uint32> Indices;
	EMeshTopology Topology = EMeshTopology::TriangleList;
};
