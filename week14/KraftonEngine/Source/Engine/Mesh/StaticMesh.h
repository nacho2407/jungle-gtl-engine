#pragma once

#include "Object/Object.h"
#include "Collision/MeshPickingBVH.h"
#include "Mesh/StaticMeshAsset.h"
#include "Serialization/Archive.h"

#include <memory>

struct ID3D11Device;

// UStaticMesh — FStaticMesh를 소유하는 UObject 에셋
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials);
	const TArray<FStaticMaterial>& GetStaticMaterials() const;

	void InitResources(ID3D11Device* InDevice);
	void EnsureMeshPickingBVHBuilt() const;
	bool RaycastMeshBVHLocal(const FVector& LocalOrigin, const FVector& LocalDirection, FHitResult& OutHitResult) const;

	// 메시 변경 대응용 dirty API는 현재 범위에서 혼동을 줄 수 있어 주석 처리합니다.
	// void MarkMeshPickingBVHDirty();
	
private:
	FStaticMesh* StaticMeshAsset = nullptr;
	TArray<FStaticMaterial> StaticMaterials; // 슬롯 이름과 머티리얼 인터페이스를 묶어서 저장하는 배열
	mutable FMeshPickingBVH MeshPickingBVH;
};
