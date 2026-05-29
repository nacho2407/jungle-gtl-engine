#include "PhysicsAsset.h"

#include "Object/GarbageCollection.h"

int32 UPhysicsAsset::FindBodyIndexByBoneName(const FName& BoneName) const
{
	for (int32 Index = 0; Index < static_cast<int32>(SkeletalBodySetups.size()); ++Index)
	{
		const USkeletalBodySetup* BodySetup = SkeletalBodySetups[Index];
		if (BodySetup && BodySetup->BoneName == BoneName)
		{
			return Index;
		}
	}

	return -1;
}

USkeletalBodySetup* UPhysicsAsset::FindBodySetupByBoneName(const FName& BoneName) const
{
	const int32 BodyIndex = FindBodyIndexByBoneName(BoneName);
	if (BodyIndex == -1)
	{
		return nullptr;
	}

	return SkeletalBodySetups[BodyIndex];
}

void UPhysicsAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	for (USkeletalBodySetup* BodySetup : SkeletalBodySetups)
	{
		Collector.AddReferencedObject(BodySetup, "UPhysicsAsset.SkeletalBodySetups");
	}

	for (UPhysicsConstraintTemplate* ConstraintTemplate : ConstraintSetup)
	{
		Collector.AddReferencedObject(ConstraintTemplate, "UPhysicsAsset.ConstraintSetup");
	}
}
