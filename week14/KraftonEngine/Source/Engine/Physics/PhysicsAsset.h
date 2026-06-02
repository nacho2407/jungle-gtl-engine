#pragma once

#include "Object/Object.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsConstraintTemplate.h"

#include "Source/Engine/Physics/PhysicsAsset.generated.h"

struct FPhysicsAssetCollisionDisablePair
{
	FName BodyA;
	FName BodyB;
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

	const TArray<FPhysicsAssetCollisionDisablePair>& GetCollisionDisablePairs() const { return CollisionDisablePairs; }
	int32 FindCollisionDisablePairIndex(FName BodyA, FName BodyB) const;
	bool IsCollisionDisabled(FName BodyA, FName BodyB) const;
	void SetCollisionDisabled(FName BodyA, FName BodyB, bool bDisabled);
	void RemoveCollisionDisablePairsForBody(FName BoneName);
	void Clear();
	
private:
	TArray<UBodySetup*> BodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintTemplates;
	TArray<FPhysicsAssetCollisionDisablePair> CollisionDisablePairs;
	TMap<FString, int32> BodySetupIndexMap;
	FString SourcePath;
	FString PreviewSkeletalMeshPath = "None";
};
