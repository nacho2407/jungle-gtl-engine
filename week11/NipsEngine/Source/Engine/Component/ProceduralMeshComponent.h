#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/VertexTypes.h"
#include "Geometry/Plane.h"
#include "Render/Common/RenderTypes.h"

class UProceduralMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UProceduralMeshComponent, UPrimitiveComponent)

    struct FMeshSection
    {
        TArray<FNormalVertex> Vertices;
        TArray<uint32> Indices;
    };

public:
    void CreateSection(int32 SectionIndex,
                       const TArray<FNormalVertex>& InVertices,
                       const TArray<uint32>& InIndices);

    void ClearSection(int32 SectionIndex);

    void ClearAllSections();

    // Primitive override
    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override;

    // Getter
    const TArray<FMeshSection>& GetSections() const { return Sections; }

private:
    TArray<FMeshSection> Sections;
};

struct FSliceMeshData
{
    TArray<FNormalVertex> Vertices;
    TArray<uint32> Indices;
};


class FMeshSlicer
{
public:
    static void Slice(
        const FSliceMeshData& InMesh,
        const FPlane& Plane,
        FSliceMeshData& OutFront,
        FSliceMeshData& OutBack);

	static void SliceComponent(
        UPrimitiveComponent* InComponent,
        const FPlane& Plane,
        UProceduralMeshComponent*& OutFront,
        UProceduralMeshComponent*& OutBack);

	static void AddTriangle(FSliceMeshData& Mesh, const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C);

	static FNormalVertex Intersect(const FNormalVertex& A, const FNormalVertex& B, float dA, float dB);
};

