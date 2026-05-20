#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"
#include "Mesh/Static/StaticMeshAsset.h"

class FFbxTangentBuilder
{
public:
	static void AccumulateSkeletalTriangle(FFbxImportContext& Context, const uint32 TriIndices[3]);
	static void BuildSkeletalTangentsForVertexRange(FFbxImportContext& Context, uint32 VertexStart);
	static void AccumulateStaticTriangle(const FStaticMesh& Mesh, const uint32 TriIndices[3], TArray<FVector>& TangentSums, TArray<FVector>& BitangentSums);
	static void FinalizeStaticTangents(FStaticMesh& Mesh, const TArray<FVector>& TangentSums, const TArray<FVector>& BitangentSums);
};
