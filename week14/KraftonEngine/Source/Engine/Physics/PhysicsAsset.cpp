#include "PhysicsAsset.h"
#include "Object/ReferenceCollector.h"

#include <algorithm>
#include <cctype>

namespace
{
	FString MakeBodySetupIndexKey(FName BoneName)
	{
		FString Key = BoneName.ToString();
		std::transform(Key.begin(), Key.end(), Key.begin(),
			[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		return Key;
	}

	FString MakeCollisionDisablePairKey(FName BodyA, FName BodyB)
	{
		FString KeyA = MakeBodySetupIndexKey(BodyA);
		FString KeyB = MakeBodySetupIndexKey(BodyB);
		if (KeyB < KeyA)
		{
			std::swap(KeyA, KeyB);
		}
		return KeyA + "|" + KeyB;
	}

	bool IsUsablePairName(FName BoneName)
	{
		return BoneName.IsValid() && BoneName != FName::None;
	}
}

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
		uint32 ExtVersion = 2;
		Ar << ExtVersion;
		Ar << PreviewSkeletalMeshPath;

		uint32 NumCollisionDisablePairs = static_cast<uint32>(CollisionDisablePairs.size());
		Ar << NumCollisionDisablePairs;
		for (FPhysicsAssetCollisionDisablePair& Pair : CollisionDisablePairs)
		{
			Ar << Pair.BodyA;
			Ar << Pair.BodyB;
		}
	}
	else if (!Ar.AtEnd())
	{
		uint32 ExtVersion = 0;
		Ar << ExtVersion;
		if (ExtVersion >= 1)
		{
			Ar << PreviewSkeletalMeshPath;
		}
		if (ExtVersion >= 2)
		{
			uint32 NumCollisionDisablePairs = 0;
			Ar << NumCollisionDisablePairs;
			CollisionDisablePairs.clear();
			CollisionDisablePairs.reserve(NumCollisionDisablePairs);
			for (uint32 i = 0; i < NumCollisionDisablePairs; ++i)
			{
				FPhysicsAssetCollisionDisablePair Pair;
				Ar << Pair.BodyA;
				Ar << Pair.BodyB;
				if (IsUsablePairName(Pair.BodyA) && IsUsablePairName(Pair.BodyB) && Pair.BodyA != Pair.BodyB)
				{
					CollisionDisablePairs.push_back(Pair);
				}
			}
		}
		else
		{
			CollisionDisablePairs.clear();
		}
	}
	else if (Ar.IsLoading())
	{
		CollisionDisablePairs.clear();
	}

	if (Ar.IsLoading())
	{
		UpdateBodySetupIndexMap();
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

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
	BodySetupIndexMap.clear();

	for (int32 Index = 0; Index < static_cast<int32>(BodySetups.size()); ++Index)
	{
		const UBodySetup* Body = BodySetups[Index];
		if (Body && Body->GetBoneName().IsValid() && Body->GetBoneName() != FName::None)
		{
			BodySetupIndexMap[MakeBodySetupIndexKey(Body->GetBoneName())] = Index;
		}
	}
}

int32 UPhysicsAsset::FindBodyIndex(FName BoneName) const
{
	const auto It = BodySetupIndexMap.find(MakeBodySetupIndexKey(BoneName));
	if (It != BodySetupIndexMap.end())
	{
		const int32 Index = It->second;
		if (Index >= 0 && Index < static_cast<int32>(BodySetups.size()))
		{
			const UBodySetup* Body = BodySetups[Index];
			if (Body && Body->GetBoneName() == BoneName)
			{
				return Index;
			}
		}
	}

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
	BodySetupIndexMap[MakeBodySetupIndexKey(BoneName)] = static_cast<int32>(BodySetups.size()) - 1;
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
	RemoveCollisionDisablePairsForBody(BoneName);

	BodySetups.erase(BodySetups.begin() + BodyIndex);
	UpdateBodySetupIndexMap();
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

UPhysicsConstraintTemplate* UPhysicsAsset::CreateConstraint(FName ParentBone, FName ChildBone, const FTransform& FrameA, const FTransform& FrameB, EAngularConstraintMode Mode, bool bDisableCollision)
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
		SetCollisionDisabled(ParentBone, ChildBone, bDisableCollision);
		return ConstraintTemplates[ExistingIndex];
	}

	UPhysicsConstraintTemplate* Constraint = UObjectManager::Get().CreateObject<UPhysicsConstraintTemplate>(this);
	Constraint->Setup(ParentBone, ChildBone, FrameA, FrameB, Mode);
	ConstraintTemplates.push_back(Constraint);
	SetCollisionDisabled(ParentBone, ChildBone, bDisableCollision);
	return Constraint;
}

bool UPhysicsAsset::RemoveConstraintAt(int32 ConstraintIndex)
{
	if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(ConstraintTemplates.size()))
	{
		return false;
	}

	UPhysicsConstraintTemplate* Constraint = ConstraintTemplates[ConstraintIndex];
	const FName ParentBone = Constraint ? Constraint->GetParentBoneName() : FName::None;
	const FName ChildBone = Constraint ? Constraint->GetChildBoneName() : FName::None;
	ConstraintTemplates.erase(ConstraintTemplates.begin() + ConstraintIndex);
	if (Constraint)
	{
		UObjectManager::Get().DestroyObject(Constraint);
	}
	SetCollisionDisabled(ParentBone, ChildBone, false);
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

int32 UPhysicsAsset::FindCollisionDisablePairIndex(FName BodyA, FName BodyB) const
{
	if (!IsUsablePairName(BodyA) || !IsUsablePairName(BodyB) || BodyA == BodyB)
	{
		return -1;
	}

	const FString TargetKey = MakeCollisionDisablePairKey(BodyA, BodyB);
	for (int32 Index = 0; Index < static_cast<int32>(CollisionDisablePairs.size()); ++Index)
	{
		const FPhysicsAssetCollisionDisablePair& Pair = CollisionDisablePairs[Index];
		if (MakeCollisionDisablePairKey(Pair.BodyA, Pair.BodyB) == TargetKey)
		{
			return Index;
		}
	}

	return -1;
}

bool UPhysicsAsset::IsCollisionDisabled(FName BodyA, FName BodyB) const
{
	return FindCollisionDisablePairIndex(BodyA, BodyB) >= 0;
}

void UPhysicsAsset::SetCollisionDisabled(FName BodyA, FName BodyB, bool bDisabled)
{
	if (!IsUsablePairName(BodyA) || !IsUsablePairName(BodyB) || BodyA == BodyB)
	{
		return;
	}

	const int32 ExistingIndex = FindCollisionDisablePairIndex(BodyA, BodyB);
	if (bDisabled)
	{
		if (ExistingIndex < 0)
		{
			CollisionDisablePairs.push_back({ BodyA, BodyB });
		}
		return;
	}

	if (ExistingIndex >= 0)
	{
		CollisionDisablePairs.erase(CollisionDisablePairs.begin() + ExistingIndex);
	}
}

void UPhysicsAsset::RemoveCollisionDisablePairsForBody(FName BoneName)
{
	if (!IsUsablePairName(BoneName))
	{
		return;
	}

	CollisionDisablePairs.erase(
		std::remove_if(
			CollisionDisablePairs.begin(),
			CollisionDisablePairs.end(),
			[BoneName](const FPhysicsAssetCollisionDisablePair& Pair)
			{
				return Pair.BodyA == BoneName || Pair.BodyB == BoneName;
			}),
		CollisionDisablePairs.end());
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
	CollisionDisablePairs.clear();
	BodySetupIndexMap.clear();
}
