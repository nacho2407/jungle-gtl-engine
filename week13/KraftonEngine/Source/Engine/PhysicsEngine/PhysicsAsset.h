#pragma once

#include "Object/Object.h"
#include "PhysicsConstraintTemplate.h"
#include "BodySetup.h"

class UPhysicsAsset : public UObject
{
public:
	UPhysicsAsset() = default;
	~UPhysicsAsset() override = default;

	const TArray<UBodySetup*>& GetBodySetups() const { return BodySetups; }

	TArray<UBodySetup*>& GetBodySetupsMutable() { return BodySetups; }

	//const TArray<UPhysicsConstraintTemplate*>& GetConstraintSetup() const { return ConstraintSetup; }

	//TArray<UPhysicsConstraintTemplate*>& GetConstraintSetupMutable() { return ConstraintSetup; }

	const TArray<UBodySetup*>& GetBodySetups() { return BodySetups; }

	TArray<UBodySetup*>& GetBodySetupsMutable() { return BodySetups; }

	int32 FindBodyIndexByBoneName(const FName& BoneName) const;
	UBodySetup* FindBodySetupByBoneName(const FName& BoneName) const;

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	TArray<UBodySetup*> BodySetups;
	//TArray<UPhysicsConstraintTemplate*> ConstraintSetup;
};
