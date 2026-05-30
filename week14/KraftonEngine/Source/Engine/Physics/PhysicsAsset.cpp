#include "PhysicsAsset.h"
#include "Object/ReferenceCollector.h"

void UPhysicsAsset::Serialize(FArchive& Ar)
{
	uint32 NumBodies = static_cast<uint32>(BodySetups.size());
	Ar << NumBodies;
	
	if (Ar.IsLoading())
	{
		BodySetups.clear();
		BodySetups.reserve(NumBodies);
		for (uint32 i = 0; i < NumBodies; ++i)
		{
			UBodySetup* Body = UObjectManager::Get().CreateObject<UBodySetup>(this);
			Body->Serialize(Ar);
			BodySetups.push_back(Body);
		}
	}
	else
	{
		for (UBodySetup* Body : BodySetups)
		{
			Body->Serialize(Ar);
		}
	}
	
	uint32 NumConstraints = static_cast<uint32>(ConstraintTemplates.size());
	Ar << NumConstraints;

	if (Ar.IsLoading())
	{
		ConstraintTemplates.clear();
		ConstraintTemplates.reserve(NumConstraints);
		for (uint32 i = 0; i < NumConstraints; ++i)
		{
			UPhysicsConstraintTemplate* Constraint = UObjectManager::Get().CreateObject<UPhysicsConstraintTemplate>(this);
			Constraint->Serialize(Ar);
			ConstraintTemplates.push_back(Constraint);
		}
	}
	else
	{
		for (UPhysicsConstraintTemplate* Constraint : ConstraintTemplates)
		{
			Constraint->Serialize(Ar);
		}
	}
}

void UPhysicsAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);
	for (UBodySetup* Body : BodySetups)
	{
		Collector.AddReferencedObject(Body);
	}
	for (UPhysicsConstraintTemplate* Constraint : ConstraintTemplates)
	{
		Collector.AddReferencedObject(Constraint);
	}
}

int32 UPhysicsAsset::FindBodyIndex(FName BoneName) const
{
	for (int32 Index = 0; Index < static_cast<int32>(BodySetups.size()); ++Index)
	{
		const UBodySetup* Body = BodySetups[Index];
		if (Body && Body->GetBoneName() == BoneName)
		{
			return Index;
		}
	}

	return -1;
}

UBodySetup* UPhysicsAsset::FindBodySetup(FName BoneName) const
{
	const int32 Index = FindBodyIndex(BoneName);
	return Index >= 0 ? BodySetups[Index] : nullptr;
}

UBodySetup* UPhysicsAsset::CreateBodySetup(FName BoneName)
{
	UBodySetup* Body = UObjectManager::Get().CreateObject<UBodySetup>(this);
	Body->SetBoneName(BoneName);
	BodySetups.push_back(Body);
	return Body;
}

UPhysicsConstraintTemplate* UPhysicsAsset::CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode)
{
	UPhysicsConstraintTemplate* Constraint = UObjectManager::Get().CreateObject<UPhysicsConstraintTemplate>(this);
	Constraint->Setup(ParentBone, ChildBone, FrameA, FrameB, Mode);
	ConstraintTemplates.push_back(Constraint);
	return Constraint;
}
