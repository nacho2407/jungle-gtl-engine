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

	// Trailing extension section. Older physics assets end before this point
	if (Ar.IsSaving())
	{
		uint32 ExtVersion = 1;
		Ar << ExtVersion;
		Ar << PreviewSkeletalMeshPath;
	}
	else if (!Ar.AtEnd())
	{
		uint32 ExtVersion = 0;
		Ar << ExtVersion;
		if (ExtVersion >= 1)
		{
			Ar << PreviewSkeletalMeshPath;
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

UBodySetup* UPhysicsAsset::GetOrCreateBodySetup(FName BoneName)
{
	if (!BoneName.IsValid() || BoneName == FName::None)
	{
		return nullptr;
	}

	if (UBodySetup* Existing = FindBodySetup(BoneName))
	{
		return Existing;
	}

	return CreateBodySetup(BoneName);
}

UBodySetup* UPhysicsAsset::CreateBodySetup(FName BoneName)
{
	if (!BoneName.IsValid() || BoneName == FName::None)
	{
		return nullptr;
	}

	if (UBodySetup* Existing = FindBodySetup(BoneName))
	{
		return Existing;
	}

	UBodySetup* Body = UObjectManager::Get().CreateObject<UBodySetup>(this);
	Body->SetBoneName(BoneName);
	BodySetups.push_back(Body);
	return Body;
}

bool UPhysicsAsset::RemoveBodySetup(FName BoneName)
{
	const int32 BodyIndex = FindBodyIndex(BoneName);
	return RemoveBodySetupAt(BodyIndex);
}

bool UPhysicsAsset::RemoveBodySetupAt(int32 BodyIndex)
{
	if (BodyIndex < 0 || BodyIndex >= static_cast<int32>(BodySetups.size()))
	{
		return false;
	}

	UBodySetup* Body = BodySetups[BodyIndex];
	const FName BoneName = Body ? Body->GetBoneName() : FName::None;
	RemoveConstraintsForBody(BoneName);

	BodySetups.erase(BodySetups.begin() + BodyIndex);
	if (Body)
	{
		UObjectManager::Get().DestroyObject(Body);
	}
	return true;
}

int32 UPhysicsAsset::FindConstraintIndex(FName ParentBone, FName ChildBone) const
{
	for (int32 Index = 0; Index < static_cast<int32>(ConstraintTemplates.size()); ++Index)
	{
		const UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[Index];
		if (Constraint
			&& Constraint->GetParentBoneName() == ParentBone
			&& Constraint->GetChildBoneName() == ChildBone)
		{
			return Index;
		}
	}

	return -1;
}

UPhysicsConstraintTemplate* UPhysicsAsset::CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode)
{
	if (!ParentBone.IsValid() || !ChildBone.IsValid() || ParentBone == FName::None || ChildBone == FName::None)
	{
		return nullptr;
	}

	if (ParentBone == ChildBone)
	{
		return nullptr;
	}

	if (!FindBodySetup(ParentBone) || !FindBodySetup(ChildBone))
	{
		return nullptr;
	}

	const int32 ExistingIndex = FindConstraintIndex(ParentBone, ChildBone);
	if (ExistingIndex >= 0)
	{
		return ConstraintTemplates[ExistingIndex];
	}

	UPhysicsConstraintTemplate* Constraint = UObjectManager::Get().CreateObject<UPhysicsConstraintTemplate>(this);
	Constraint->Setup(ParentBone, ChildBone, FrameA, FrameB, Mode);
	ConstraintTemplates.push_back(Constraint);
	return Constraint;
}

bool UPhysicsAsset::RemoveConstraintAt(int32 ConstraintIndex)
{
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(ConstraintTemplates.size()))
	{
		return false;
	}

	UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[ConstraintIndex];
	ConstraintTemplates.erase(ConstraintTemplates.begin() + ConstraintIndex);
	if (Constraint)
	{
		UObjectManager::Get().DestroyObject(Constraint);
	}
	return true;
}

void UPhysicsAsset::RemoveConstraintsForBody(FName BoneName)
{
	for (int32 Index = static_cast<int32>(ConstraintTemplates.size()) - 1; Index >= 0; --Index)
	{
		const UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[Index];
		if (!Constraint
			|| Constraint->GetParentBoneName() == BoneName
			|| Constraint->GetChildBoneName() == BoneName)
		{
			RemoveConstraintAt(Index);
		}
	}
}

void UPhysicsAsset::Clear()
{
	for (UPhysicsConstraintTemplate* Constraint : ConstraintTemplates)
	{
		if (Constraint)
		{
			UObjectManager::Get().DestroyObject(Constraint);
		}
	}
	ConstraintTemplates.clear();

	for (UBodySetup* Body : BodySetups)
	{
		if (Body)
		{
			UObjectManager::Get().DestroyObject(Body);
		}
	}
	BodySetups.clear();
}
