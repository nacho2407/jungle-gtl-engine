#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/VertexTypes.h"
#include "Geometry/Plane.h"
#include "Render/Common/RenderTypes.h"

class UStaticMesh;
class UStaticMeshComponent;

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
    void CreateFrom(UStaticMesh* StaticMesh);
    void CreateFrom(UProceduralMeshComponent* ProcMeshComp);

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

    /* For Material */
    int32 GetNumMaterials() const override { return Materials.size(); }
    class UMaterialInterface* GetMaterial(int32 SlotIndex) const override;
    void SetMaterial(int32 SlotIndex, class UMaterialInterface* InMaterial) override;

    void PostDuplicate(UObject* Original) override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
    TArray<FMeshSection> Sections;
    TArray<UMaterialInterface*> Materials;
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
        UStaticMeshComponent* InComponent,
        const FPlane& Plane,
        UProceduralMeshComponent*& OutFront,
        UProceduralMeshComponent*& OutBack);

	static void SliceComponent(
        UProceduralMeshComponent* InComponent,
        const FPlane& Plane,
        UProceduralMeshComponent*& OutFront,
        UProceduralMeshComponent*& OutBack);

	static void AddTriangle(FSliceMeshData& Mesh, const FNormalVertex& A, const FNormalVertex& B, const FNormalVertex& C);

	static FNormalVertex Intersect(const FNormalVertex& A, const FNormalVertex& B, float dA, float dB);

	static void BuildCapMesh(
							TArray<FNormalVertex> CapLoop, // 값 복사로 받기
							const FVector& Normal,
							bool bFlipWinding,
							TArray<FNormalVertex>& OutVertices,
							TArray<uint32>& OutIndices);
};

