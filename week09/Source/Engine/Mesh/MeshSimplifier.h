// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Mesh/StaticMeshAsset.h"

// FSimplifiedMesh는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct FSimplifiedMesh
{
    TArray<FVertexPNCT_T> Vertices;
    TArray<uint32> Indices;
    TArray<FStaticMeshSection> Sections;
};

// FMeshSimplifier는 메시 데이터와 렌더 제출 정보를 다룹니다.
class FMeshSimplifier
{
public:
    // TargetRatio: 0.5 = 삼각형 50%, 0.25 = 25%
    static FSimplifiedMesh Simplify(
        const TArray<FVertexPNCT_T>& InVertices,
        const TArray<uint32>& InIndices,
        const TArray<FStaticMeshSection>& InSections,
        float TargetRatio);
};
