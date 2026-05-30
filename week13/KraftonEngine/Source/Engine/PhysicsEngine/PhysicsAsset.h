#pragma once

#include "Object/Object.h"
#include "BodySetup.h"

class UPhysicsAsset : public UObject
{
public:
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	const TArray<UBodySetup*>& GetBodySetups() const { return BodySetups; }
	TArray<UBodySetup*>& GetBodySetupsMutable() { return BodySetups; }

	const FString& GetAssetPathFileName() const { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InAssetPathFileName) { AssetPathFileName = InAssetPathFileName; }

	const FString& GetSourceSkeletalMeshPath() const { return SourceSkeletalMeshPath; }
	void SetSourceSkeletalMeshPath(const FString& InSourceSkeletalMeshPath) { SourceSkeletalMeshPath = InSourceSkeletalMeshPath; }

	int32 FindBodyIndexByBoneName(const FName& BoneName) const;
	UBodySetup* FindBodySetupByBoneName(const FName& BoneName) const;

	void Serialize(FArchive& Ar) override;
	void SerializeLegacyEmbedded(FArchive& Ar, uint32 SerializedObjectNameLength);
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	FString AssetPathFileName = "None";
	FString SourceSkeletalMeshPath = "None";
	TArray<UBodySetup*> BodySetups;
};
