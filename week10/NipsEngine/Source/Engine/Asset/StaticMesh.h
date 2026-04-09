#pragma once
#pragma once

#include "StaticMeshTypes.h"
#include "Object/Object.h"

class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override;

	void SetMeshData(FStaticMesh* InMeshData, const TArray<FStaticMeshMaterialSlot>& MaterialSlot);

	/* Getters */
	FStaticMesh* GetMeshData(int32 LOD = 0);
	const FStaticMesh* GetMeshData(int32 LOD = 0) const;

	const FString& GetAssetPathFileName() const;

	const TArray<FNormalVertex>& GetVertices() const;
	const TArray<uint32>& GetIndices() const;

	const TArray<FStaticMeshSection>& GetSections() const;
	const TArray<FStaticMeshMaterialSlot>& GetMaterialSlots() const;

	const FAABB& GetLocalBounds() const;
	
	bool HasValidMeshData() const;
	const int32 GetValidLODCount() const { return ValidLODCount; }
	
private:
	void RebuildLocalBoundsFromMeshData();

private:
	FStaticMesh* MeshData = nullptr;
	TArray<FStaticMeshMaterialSlot> MaterialSlots;

	// simplifer에서 접근할 수 있도록 friend class 선언한다.
	friend class FStaticMeshSimplifier;

	int32 ValidLODCount = 1;
	
	// 최대 LOD 레벨을 양쪽에서 저장하고 있습니다... 
	// MAX_LOD를 수정하실 필요가 있다면 MeshBufferManager를 찾아 함께 수정해주세요.
	static constexpr int32 MAX_LOD = 5;
	FStaticMesh* LODMeshData[MAX_LOD] = { nullptr };
};
