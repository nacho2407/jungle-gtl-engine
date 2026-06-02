#pragma once

#include "Object/Object.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsConstraintTemplate.h"

#include "Source/Engine/Physics/PhysicsAsset.generated.h"

struct FPhysicsAssetBodyProfileData
{
	FName BoneName = FName::None;
	bool bCollisionEnabled = true;
	FString PhysicalMaterialPath = "None";
};

struct FPhysicsAssetCollisionDisablePair
{
	FName BoneA = FName::None;
	FName BoneB = FName::None;
};

struct FPhysicsAssetGraphNodePosition
{
	FName BoneName = FName::None;
	float X = 0.0f;
	float Y = 0.0f;
};

UCLASS()
class UPhysicsAsset : public UObject
{
public:
	GENERATED_BODY()
	
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	void SetSourcePath(const FString& InPath) {SourcePath = InPath;}
	const FString& GetSourcePath() const {return SourcePath;}

	void SetPreviewSkeletalMeshPath(const FString& InPath) { PreviewSkeletalMeshPath = InPath; }
	const FString& GetPreviewSkeletalMeshPath() const { return PreviewSkeletalMeshPath; }
	
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	const TArray<UBodySetup*>& GetBodySetups() const {return BodySetups;}
	const TArray<FPhysicsAssetBodyProfileData>& GetBodyProfiles() const { return BodyProfiles; }
	const TArray<FPhysicsAssetCollisionDisablePair>& GetDisabledCollisionPairs() const { return DisabledCollisionPairs; }
	
	void UpdateBodySetupIndexMap();
	int32 FindBodyIndex(FName BoneName) const;
	UBodySetup* FindBodySetup(FName BoneName) const;
	UBodySetup* GetOrCreateBodySetup(FName BoneName);
	UBodySetup* CreateBodySetup(FName BoneName);
	bool RemoveBodySetup(FName BoneName);
	bool RemoveBodySetupAt(int32 BodyIndex);
	
	const TArray<UPhysicsConstraintTemplate*>& GetConstraintTemplates() const {return ConstraintTemplates;}
	int32 FindConstraintIndex(FName ParentBone, FName ChildBone) const;
	UPhysicsConstraintTemplate* CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode, bool bDisableCollision = true);
	bool RemoveConstraintAt(int32 ConstraintIndex);
	void RemoveConstraintsForBody(FName BoneName);

	bool IsBodyCollisionEnabled(FName BoneName) const;
	void SetBodyCollisionEnabled(FName BoneName, bool bEnabled);
	const FString& GetBodyPhysicalMaterialPath(FName BoneName) const;
	void SetBodyPhysicalMaterialPath(FName BoneName, const FString& InPath);
	bool IsCollisionDisabled(FName BoneA, FName BoneB) const;
	void SetCollisionDisabled(FName BoneA, FName BoneB, bool bDisabled);
	bool GetGraphNodePosition(FName BoneName, float& OutX, float& OutY) const;
	void SetGraphNodePosition(FName BoneName, float X, float Y);

	const TArray<FPhysicsAssetCollisionDisablePair>& GetCollisionDisablePairs() const { return DisabledCollisionPairs; }
	int32 FindCollisionDisablePairIndex(FName BoneA, FName BoneB) const;
	void RemoveCollisionDisablePairsForBody(FName BoneName);
	void Clear();
	
private:
	FPhysicsAssetBodyProfileData* FindMutableBodyProfile(FName BoneName);
	const FPhysicsAssetBodyProfileData* FindBodyProfile(FName BoneName) const;
	void RemoveEditorDataForBody(FName BoneName);

	TArray<UBodySetup*> BodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintTemplates;
	TArray<FPhysicsAssetBodyProfileData> BodyProfiles;
	TArray<FPhysicsAssetCollisionDisablePair> DisabledCollisionPairs;
	TArray<FPhysicsAssetGraphNodePosition> GraphNodePositions;
	TMap<FString, int32> BodySetupIndexMap;
	FString SourcePath;
	FString PreviewSkeletalMeshPath = "None";
};
