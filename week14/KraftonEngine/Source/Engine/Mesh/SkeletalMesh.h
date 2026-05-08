#pragma once

#include "Object/Object.h"
#include "SkeletalMeshAsset.h"

class USkeletalMesh : public UObject
{
public:
	DECLARE_CLASS(USkeletalMesh, UObject)

	USkeletalMesh() = default;
	~USkeletalMesh() override = default;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const;
	void SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
	FSkeletalMesh* GetSkeletalMeshAsset() const;
	void SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials);
	const TArray<FSkeletalMaterial>& GetSkeletalMaterials() const;

	void InitResources(ID3D11Device* InDevice);

private:
	FSkeletalMesh* SkeletalMeshAsset = nullptr;
	TArray<FSkeletalMaterial> SkeletalMaterials;
};
