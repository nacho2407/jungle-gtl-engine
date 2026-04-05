#pragma once

#include "Object/Object.h"
#include "Collision/MeshTrianglePickingBVH.h"
#include "Mesh/StaticMeshAsset.h"
#include "Serialization/Archive.h"

#include <memory>

struct ID3D11Device;

// LOD 단계별 GPU 리소스
struct FLODMeshData
{
	TArray<FStaticMeshSection> Sections;
	std::unique_ptr<FMeshBuffer> RenderBuffer;
};

// UStaticMesh — FStaticMesh를 소유하는 UObject 에셋
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	static constexpr uint32 MAX_LOD_COUNT = 4;

	UStaticMesh() = default;
	~UStaticMesh() override;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials);
	const TArray<FStaticMaterial>& GetStaticMaterials() const;

	void InitResources(ID3D11Device* InDevice);

	//스태틱 메시 picking 최적화를 위한 BVH 트리 빌드 및 판정 호출 함수
	void EnsureMeshTrianglePickingBVHBuilt() const;
	bool RaycastMeshTrianglesWithBVHLocal(const FVector& LocalOrigin, const FVector& LocalDirection, FHitResult& OutHitResult) const;
	const FMeshTrianglePickingBVH::FTraversalMetrics& GetLastMeshPickingMetrics() const { return MeshTrianglePickingBVH.GetLastTraversalMetrics(); }

	// LOD 접근
	uint32 GetLODCount() const { return bHasLOD ? MAX_LOD_COUNT : 1; }
	FMeshBuffer* GetLODMeshBuffer(uint32 LODLevel) const;
	const TArray<FStaticMeshSection>& GetLODSections(uint32 LODLevel) const;

private:
	FStaticMesh* StaticMeshAsset = nullptr;
	TArray<FStaticMaterial> StaticMaterials; // 슬롯 이름과 머티리얼 인터페이스를 묶어서 저장하는 배열
	mutable FMeshTrianglePickingBVH MeshTrianglePickingBVH; // 빠른 picking을 위해 메시 내부에 트리 형태로 만들어지는 자료구조

	// LOD1 (70%), LOD2 (50%), LOD3 (25%) — LOD0 is the original StaticMeshAsset
	FLODMeshData AdditionalLODs[3];
	bool bHasLOD = false;
};
