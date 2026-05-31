#pragma once

#include "Object/Object.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsConstraintTemplate.h"

#include "Source/Engine/Physics/PhysicsAsset.generated.h"

UCLASS()
class UPhysicsAsset : public UObject
{
public:
	GENERATED_BODY()
	
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	void SetSourcePath(const FString& InPath) {SourcePath = InPath;}
	const FString& GetSourcePath() const {return SourcePath;}
	
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	const TArray<UBodySetup*>& GetBodySetups() const {return BodySetups;}
	TArray<UBodySetup*>& GetMutableBodySetups() { return BodySetups; }
	
	int32 FindBodyIndex(FName BoneName) const;
	UBodySetup* FindBodySetup(FName BoneName) const;
	UBodySetup* GetOrCreateBodySetup(FName BoneName);
	UBodySetup* CreateBodySetup(FName BoneName);
	bool RemoveBodySetup(FName BoneName);
	bool RemoveBodySetupAt(int32 BodyIndex);
	
	const TArray<UPhysicsConstraintTemplate*>& GetConstraintTemplates() const {return ConstraintTemplates;}
	TArray<UPhysicsConstraintTemplate*>& GetMutableConstraintTemplates() { return ConstraintTemplates; }
	int32 FindConstraintIndex(FName ParentBone, FName ChildBone) const;
	UPhysicsConstraintTemplate* CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode);
	bool RemoveConstraintAt(int32 ConstraintIndex);
	void RemoveConstraintsForBody(FName BoneName);
	void Clear();
	
private:
	TArray<UBodySetup*> BodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintTemplates;
	FString SourcePath;
};
