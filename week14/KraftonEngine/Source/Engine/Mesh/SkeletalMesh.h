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

private:
	FSkeletalMesh* SkeletalMeshAsset = nullptr;
};
