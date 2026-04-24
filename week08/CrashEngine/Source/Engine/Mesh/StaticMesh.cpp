// 메시 영역의 세부 동작을 구현합니다.
#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Serialization/WindowsArchive.h"
#include "Mesh/ObjImporter.h"
#include "Texture/Texture2D.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Mesh/MeshSimplifier.h"

IMPLEMENT_CLASS(UStaticMesh, UObject)

static const FString EmptyPath;

UStaticMesh::~UStaticMesh()
{
    if (StaticMeshAsset)
    {
        const uint32 CPUSize =
            static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FVertexPNCT_T)) +
            static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));

        MemoryStats::SubStaticMeshCPUMemory(CPUSize);
    }
}

void UStaticMesh::Serialize(FArchive& Ar)
{
    // 에셋이 비어있으면 로드용으로 생성
    if (Ar.IsLoading() && !StaticMeshAsset)
    {
        StaticMeshAsset = new FStaticMesh();
    }

    // 1. 지오메트리 데이터 직렬화
    StaticMeshAsset->Serialize(Ar);

    // 2. 머티리얼 데이터 직렬화 (필수!)
    Ar << StaticMaterials;

    // 3. 로딩 시 Section → MaterialIndex 매핑 캐싱 (매 프레임 문자열 비교 방지)
    if (Ar.IsLoading())
    {
        for (FStaticMeshSection& Section : StaticMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
    }
}

void UStaticMesh::InitResources(ID3D11Device* InDevice)
{
    if (!InDevice || !StaticMeshAsset)
        return;

    // CPU 메모리 추적
    const uint32 CPUSize =
        static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FVertexPNCT_T)) +
        static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));
    MemoryStats::AddStaticMeshCPUMemory(CPUSize);

    // CPU → GPU 정점 버퍼 변환
    TMeshData<FVertexPNCT_T> RenderMeshData;
    RenderMeshData.Vertices.reserve(StaticMeshAsset->Vertices.size());

    for (const FVertexPNCT_T& RawVert : StaticMeshAsset->Vertices)
    {
        FVertexPNCT_T RenderVert;
        RenderVert.Position = RawVert.Position;
        RenderVert.Normal = RawVert.Normal;
        RenderVert.Color = RawVert.Color;
        RenderVert.UV = RawVert.UV;
        RenderVert.Tangent = RawVert.Tangent;
        RenderMeshData.Vertices.push_back(RenderVert);
    }
    RenderMeshData.Indices = StaticMeshAsset->Indices;

    StaticMeshAsset->RenderBuffer = std::make_unique<FMeshBuffer>();
    StaticMeshAsset->RenderBuffer->Create(InDevice, RenderMeshData);

    // ── LOD 생성 (LOD1: 90%, LOD2: 55%, LOD3: 15%) ──
    /*if (StaticMeshAsset->Vertices.size() >= 100)
    {
        static const float LODRatios[] = { 0.9f, 0.55f, 0.15f };
        for (int lod = 0; lod < 3; ++lod)
        {
            FSimplifiedMesh Simplified = FMeshSimplifier::Simplify(
                StaticMeshAsset->Vertices, StaticMeshAsset->Indices,
                StaticMeshAsset->Sections, LODRatios[lod]);

            AdditionalLODs[lod].Sections = std::move(Simplified.Sections);

            TMeshData<FVertexPNCT_T> LODRenderData;
            LODRenderData.Vertices.reserve(Simplified.Vertices.size());
            for (const FVertexPNCT_T& RawVert : Simplified.Vertices)
            {
                FVertexPNCT_T RenderVert;
                RenderVert.Position = RawVert.Position;
                RenderVert.Normal = RawVert.Normal;
                RenderVert.Color = RawVert.Color;
                RenderVert.UV = RawVert.UV;
                LODRenderData.Vertices.push_back(RenderVert);
            }
            LODRenderData.Indices = std::move(Simplified.Indices);

            AdditionalLODs[lod].RenderBuffer = std::make_unique<FMeshBuffer>();
            AdditionalLODs[lod].RenderBuffer->Create(InDevice, LODRenderData);
        }
        bHasLOD = true;
    }*/
}

const FString& UStaticMesh::GetAssetPathFileName() const
{
    if (StaticMeshAsset)
    {
        return StaticMeshAsset->PathFileName;
    }
    return EmptyPath;
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InMesh)
{
    StaticMeshAsset = InMesh;
    // 현재는 static mesh asset이 로드 후 고정된다고 보고, 메시 변경 dirty 갱신은 비활성화합니다.
    // MarkMeshTrianglePickingBVHDirty();

    // Section → MaterialIndex 캐싱 갱신
    if (StaticMeshAsset)
    {
        for (FStaticMeshSection& Section : StaticMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
        EnsureMeshTrianglePickingBVHBuilt();
    }
}

FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
    return StaticMeshAsset;
}

void UStaticMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
    StaticMaterials = InMaterials;
}

const TArray<FStaticMaterial>& UStaticMesh::GetStaticMaterials() const
{
    return StaticMaterials;
}

void UStaticMesh::EnsureMeshTrianglePickingBVHBuilt() const
{
    if (!StaticMeshAsset)
    {
        return;
    }

    MeshTrianglePickingBVH.EnsureBuilt(*StaticMeshAsset);
}

bool UStaticMesh::RaycastMeshTrianglesWithBVHLocal(const FVector& LocalOrigin, const FVector& LocalDirection, FHitResult& OutHitResult) const
{
    if (!StaticMeshAsset)
    {
        return false;
    }

    EnsureMeshTrianglePickingBVHBuilt();
    return MeshTrianglePickingBVH.RaycastLocal(LocalOrigin, LocalDirection, *StaticMeshAsset, OutHitResult);
}

FMeshBuffer* UStaticMesh::GetLODMeshBuffer(uint32 LODLevel) const
{
    if (LODLevel == 0 && StaticMeshAsset)
        return StaticMeshAsset->RenderBuffer.get();
    if (LODLevel >= 1 && LODLevel <= 3 && bHasLOD)
        return AdditionalLODs[LODLevel - 1].RenderBuffer.get();
    return StaticMeshAsset ? StaticMeshAsset->RenderBuffer.get() : nullptr;
}

static const TArray<FStaticMeshSection> EmptySections;

const TArray<FStaticMeshSection>& UStaticMesh::GetLODSections(uint32 LODLevel) const
{
    if (LODLevel == 0 && StaticMeshAsset)
        return StaticMeshAsset->Sections;
    if (LODLevel >= 1 && LODLevel <= 3 && bHasLOD)
        return AdditionalLODs[LODLevel - 1].Sections;
    return StaticMeshAsset ? StaticMeshAsset->Sections : EmptySections;
}
