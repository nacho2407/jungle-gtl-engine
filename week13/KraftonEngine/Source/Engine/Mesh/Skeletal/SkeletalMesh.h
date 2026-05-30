#pragma once

#include "Object/Object.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Animation/Skeleton/SkeletonTypes.h"
#include "Engine/PhysicsEngine/PhysicsAsset.h"

class USkeleton;


#include "Source/Engine/Mesh/Skeletal/SkeletalMesh.generated.h"

UCLASS()
class USkeletalMesh : public UObject
{
public:
	GENERATED_BODY()
	USkeletalMesh() = default;
	~USkeletalMesh() override = default;

    void Serialize(FArchive& Ar) override;
    void AddReferencedObjects(FReferenceCollector& Collector) override;

    const FString& GetAssetPathFileName() const
    {
        return AssetPathFileName;
    }

    void SetAssetPathFileName(const FString& InPathFileName)
    {
        AssetPathFileName = InPathFileName;
    }

    void                             SetSkeletalMeshAsset(FSkeletalMesh* InMesh);
    FSkeletalMesh*                   GetSkeletalMeshAsset() const;
    void                             SetSkeletalMaterials(TArray<FSkeletalMaterial>&& InMaterials);
    const TArray<FSkeletalMaterial>& GetSkeletalMaterials() const;

    void InitResources(ID3D11Device* InDevice);

    void       SetSkeleton(USkeleton* InSkeleton);
    USkeleton* GetSkeleton() const;

    void SetSkeletonBinding(const FSkeletonBinding& InBinding);
	const FSkeletonBinding& GetSkeletonBinding() const { return SkeletonBinding; }

	UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset; }
	void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset);

	const FString& GetPhysicsAssetPath() const { return PhysicsAssetPath; }
	void SetPhysicsAssetPath(const FString& InPhysicsAssetPath) { PhysicsAssetPath = InPhysicsAssetPath; }

private:
    void CacheSectionMaterialIndices();
    void SyncSkeletonBindingToAsset();
    void SyncSkeletonBindingFromAsset();

private:
    FString AssetPathFileName = "None";

    FSkeletalMesh*            SkeletalMeshAsset = nullptr;
    TArray<FSkeletalMaterial> SkeletalMaterials;

    FSkeletonBinding SkeletonBinding;
    USkeleton*       Skeleton = nullptr;

	UPhysicsAsset* PhysicsAsset = nullptr;
	FString PhysicsAssetPath = "None";
};
