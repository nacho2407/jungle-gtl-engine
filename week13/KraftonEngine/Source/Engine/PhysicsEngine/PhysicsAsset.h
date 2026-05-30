#pragma once

#include "Object/Object.h"
#include "BodySetup.h"
#include "Physics/ConstraintInstance.h"

#include "Source/Engine/PhysicsEngine/PhysicsAsset.generated.h"

UCLASS()
class UPhysicsAsset : public UObject
{
public:
	GENERATED_BODY()
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	const TArray<UBodySetup*>& GetBodySetups() const { return BodySetups; }
	TArray<UBodySetup*>& GetBodySetupsMutable() { return BodySetups; }
	const TArray<FConstraintInstanceInitDesc>& GetConstraintInitDescs() const { return ConstraintInitDescs; }
	TArray<FConstraintInstanceInitDesc>& GetConstraintInitDescsMutable() { return ConstraintInitDescs; }

	const FString& GetAssetPathFileName() const { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InAssetPathFileName) { AssetPathFileName = InAssetPathFileName; }

	const FString& GetSourceSkeletalMeshPath() const { return SourceSkeletalMeshPath; }
	void SetSourceSkeletalMeshPath(const FString& InSourceSkeletalMeshPath) { SourceSkeletalMeshPath = InSourceSkeletalMeshPath; }

	int32 FindBodyIndexByBoneName(const FName& BoneName) const;
	UBodySetup* FindBodySetupByBoneName(const FName& BoneName) const;
	const FConstraintInstanceInitDesc* FindConstraintInitDescByChildBoneName(const FName& ChildBoneName) const;
	FConstraintInstanceInitDesc* FindConstraintInitDescByChildBoneName(const FName& ChildBoneName);

	void Serialize(FArchive& Ar) override;
	void SerializeLegacyEmbedded(FArchive& Ar, uint32 SerializedObjectNameLength);
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	FString AssetPathFileName = "None";
	FString SourceSkeletalMeshPath = "None";
	TArray<UBodySetup*> BodySetups;
	TArray<FConstraintInstanceInitDesc> ConstraintInitDescs;
};
