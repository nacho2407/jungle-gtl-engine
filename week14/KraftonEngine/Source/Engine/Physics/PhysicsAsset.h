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
	
	int32 FindBodyIndex(FName BoneName) const;
	UBodySetup* FindBodySetup(FName BoneName) const;
	UBodySetup* CreateBodySetup(FName BoneName);
	
	const TArray<UPhysicsConstraintTemplate*>& GetConstraintTemplates() const {return ConstraintTemplates;}
	UPhysicsConstraintTemplate* CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode);
	
private:
	TArray<UBodySetup*> BodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintTemplates;
	FString SourcePath;
};
