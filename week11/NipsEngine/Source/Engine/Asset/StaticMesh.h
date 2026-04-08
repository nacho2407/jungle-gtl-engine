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
	FStaticMesh* GetMeshData();
	const FStaticMesh* GetMeshData() const;

	const FString& GetAssetPathFileName() const;

	const TArray<FNormalVertex>& GetVertices() const;
	const TArray<uint32>& GetIndices() const;

	const TArray<FStaticMeshSection>& GetSections() const;
	const TArray<FStaticMeshMaterialSlot>& GetMaterialSlots() const;

	const FAABB& GetLocalBounds() const;
	
	/* */
	bool HasValidMeshData() const;
	
private:
	void RebuildLocalBoundsFromMeshData();

private:
	FStaticMesh* MeshData = nullptr;
	TArray<FStaticMeshMaterialSlot> MaterialSlots;
};
