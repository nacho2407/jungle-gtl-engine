#pragma once
#include "MeshComponent.h"

class USkeletalMesh;
class UMaterial;

class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const;
	void UpdateWorldAABB() const override;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }

protected:
	void CacheLocalBounds();

protected:
	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	TArray<FMaterialSlot> MaterialSlots;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
