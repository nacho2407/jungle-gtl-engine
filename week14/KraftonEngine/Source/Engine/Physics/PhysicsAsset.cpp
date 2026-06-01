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

UBodySetup* UPhysicsAsset::FindOrCreateBodySetup(FName BoneName)
{
	if (UBodySetup* ExistingBody = FindBodySetup(BoneName))
	{
		return ExistingBody;
	}

	return CreateBodySetup(BoneName);
}

UBodySetup* UPhysicsAsset::CreateBodySetup(FName BoneName)
{
	UBodySetup* Body = UObjectManager::Get().CreateObject<UBodySetup>(this);
	Body->SetBoneName(BoneName);
	BodySetups.push_back(Body);
	return Body;
}

bool UPhysicsAsset::RemoveBodySetup(FName BoneName)
{
	const int32 Index = FindBodyIndex(BoneName);
	if (Index < 0)
	{
		return false;
	}

	RemoveConstraintsForBone(BoneName);

	UBodySetup* RemovedBody = BodySetups[Index];
	BodySetups.erase(BodySetups.begin() + Index);
	UObjectManager::Get().DestroyObject(RemovedBody);
	return true;
}

int32 UPhysicsAsset::FindConstraintIndex(FName ParentBone, FName ChildBone) const
{
	for (int32 Index = 0; Index < static_cast<int32>(ConstraintTemplates.size()); ++Index)
	{
		const UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[Index];
		if (!Constraint)
		{
			continue;
		}

		if (Constraint->GetParentBoneName() == ParentBone &&
			Constraint->GetChildBoneName() == ChildBone)
		{
			return Index;
		}
	}

	return -1;
}

UPhysicsConstraintTemplate* UPhysicsAsset::FindConstraint(FName ParentBone, FName ChildBone) const
{
	const int32 Index = FindConstraintIndex(ParentBone, ChildBone);
	return Index >= 0 ? ConstraintTemplates[Index] : nullptr;
}

bool UPhysicsAsset::HasConstraint(FName ParentBone, FName ChildBone) const
{
	return FindConstraintIndex(ParentBone, ChildBone) >= 0;
}

UPhysicsConstraintTemplate* UPhysicsAsset::CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode)
{
	UPhysicsConstraintTemplate* Constraint = UObjectManager::Get().CreateObject<UPhysicsConstraintTemplate>(this);
	Constraint->Setup(ParentBone, ChildBone, FrameA, FrameB, Mode);
	ConstraintTemplates.push_back(Constraint);
	return Constraint;
}

bool UPhysicsAsset::RemoveConstraint(FName ParentBone, FName ChildBone)
{
	const int32 Index = FindConstraintIndex(ParentBone, ChildBone);
	return RemoveConstraintAt(Index);
}

bool UPhysicsAsset::RemoveConstraintAt(int32 ConstraintIndex)
{
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(ConstraintTemplates.size()))
	{
		return false;
	}

	UPhysicsConstraintTemplate* RemovedConstraint = ConstraintTemplates[ConstraintIndex];
	ConstraintTemplates.erase(ConstraintTemplates.begin() + ConstraintIndex);
	UObjectManager::Get().DestroyObject(RemovedConstraint);
	return true;
}

int32 UPhysicsAsset::RemoveConstraintsForBone(FName BoneName)
{
	int32 RemovedCount = 0;

	for (int32 Index = static_cast<int32>(ConstraintTemplates.size()) - 1; Index >= 0; --Index)
	{
		const UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[Index];
		if (!Constraint)
		{
			continue;
		}

		if (Constraint->GetParentBoneName() == BoneName ||
			Constraint->GetChildBoneName() == BoneName)
		{
			if (RemoveConstraintAt(Index))
			{
				++RemovedCount;
			}
		}
	}

	return RemovedCount;
}
