#pragma once

#include "Object/Object.h"
#include "Asset/StaticMeshTypes.h"

class FObjLoader;

class UStaticMesh : public UObject
{
	friend class FObjLoader;
private:
	FStaticMesh MeshData;

public:
	DECLARE_CLASS(UStaticMesh, UObject)

public:
	const TArray<FNormalVertex>&			 GetVertices()      const { return MeshData.Vertices; }
	const TArray<uint32>&					 GetIndices()       const { return MeshData.Indices; }
	const TArray<FStaticMeshSection>&		 GetSections()      const { return MeshData.Sections; }
	const TArray<FStaticMeshMaterialSlot>&   GetMaterialSlots() const { return MeshData.MaterialSlots; }
	const FString&							 GetPathFileName()  const { return MeshData.PathFileName; }

	int32 GetMaterialSlotCount() const { return static_cast<int32>(MeshData.MaterialSlots.size()); }

};
