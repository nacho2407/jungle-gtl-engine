#pragma once

#include "Object/Object.h"
#include "PhysicsConstraintTemplate.h"
#include "SkeletalBodySetup.h"

class UPhysicsAsset : public UObject
{
public:
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	const TArray<USkeletalBodySetup*>& GetSkeletalBodySetups() const
	{
		return SkeletalBodySetups;
	}

	TArray<USkeletalBodySetup*>& GetSkeletalBodySetupsMutable()
	{
		return SkeletalBodySetups;
	}

	const TArray<UPhysicsConstraintTemplate*>& GetConstraintSetup() const
	{
		return ConstraintSetup;
	}

	TArray<UPhysicsConstraintTemplate*>& GetConstraintSetupMutable()
	{
		return ConstraintSetup;
	}

	const TArray<USkeletalBodySetup*>& GetBodySetups() const
	{
		return SkeletalBodySetups;
	}

	TArray<USkeletalBodySetup*>& GetBodySetupsMutable()
	{
		return SkeletalBodySetups;
	}

	int32 FindBodyIndexByBoneName(const FName& BoneName) const;
	USkeletalBodySetup* FindBodySetupByBoneName(const FName& BoneName) const;

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	TArray<USkeletalBodySetup*> SkeletalBodySetups;
	TArray<UPhysicsConstraintTemplate*> ConstraintSetup;
};
