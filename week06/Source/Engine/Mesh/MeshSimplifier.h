#pragma once

#include "Mesh/StaticMeshAsset.h"

struct FSimplifiedMesh
{
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
};

// Edge-collapse 기반 메시 간소화
class FMeshSimplifier
{
public:
	// TargetRatio: 0.5 = 삼각형 50%, 0.25 = 25%
	static FSimplifiedMesh Simplify(
		const TArray<FNormalVertex>& InVertices,
		const TArray<uint32>& InIndices,
		const TArray<FStaticMeshSection>& InSections,
		float TargetRatio);
};
