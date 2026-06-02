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

	void NormalizeBonePair(FName& BoneA, FName& BoneB)
	{
		if (MakeBodySetupIndexKey(BoneB) < MakeBodySetupIndexKey(BoneA))
		{
			std::swap(BoneA, BoneB);
		}
	}

	bool IsValidPhysicsBoneName(FName BoneName)
	{
		return BoneName.IsValid() && BoneName != FName::None;
	}

	void SerializeBodyProfile(FArchive& Ar, FPhysicsAssetBodyProfileData& Profile)
	{
		Ar << Profile.BoneName;
		Ar << Profile.bCollisionEnabled;
		Ar << Profile.PhysicalMaterialPath;
	}

	void SerializeDisabledCollisionPair(FArchive& Ar, FPhysicsAssetCollisionDisablePair& Pair)
	{
		Ar << Pair.BoneA;
		Ar << Pair.BoneB;
	}

	void SerializeGraphNodePosition(FArchive& Ar, FPhysicsAssetGraphNodePosition& Position)
	{
		Ar << Position.BoneName;
		Ar << Position.X;
		Ar << Position.Y;
	}

	template<typename TItem, typename TSerializeFn>
	void SerializeExtensionArray(FArchive& Ar, TArray<TItem>& Array, TSerializeFn SerializeItem)
	{
		uint32 NumItems = static_cast<uint32>(Array.size());
		Ar << NumItems;

		if (Ar.IsLoading())
		{
			Array.clear();
			Array.resize(NumItems);
		}

		for (TItem& Item : Array)
		{
			SerializeItem(Ar, Item);
		}
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

	if (Ar.IsLoading())
	{
		PreviewSkeletalMeshPath = "None";
		BodyProfiles.clear();
		DisabledCollisionPairs.clear();
		GraphNodePositions.clear();
	}

	// Trailing extension section. Older physics assets end before this point
	if (Ar.IsSaving())
	{
		uint32 ExtVersion = 2;
		Ar << ExtVersion;
		Ar << PreviewSkeletalMeshPath;
		SerializeExtensionArray(Ar, BodyProfiles, SerializeBodyProfile);
		SerializeExtensionArray(Ar, DisabledCollisionPairs, SerializeDisabledCollisionPair);
		SerializeExtensionArray(Ar, GraphNodePositions, SerializeGraphNodePosition);
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
			SerializeExtensionArray(Ar, BodyProfiles, SerializeBodyProfile);
			SerializeExtensionArray(Ar, DisabledCollisionPairs, SerializeDisabledCollisionPair);
			SerializeExtensionArray(Ar, GraphNodePositions, SerializeGraphNodePosition);
		}
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
	SetBodyCollisionEnabled(BoneName, true);
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
	UpdateBodySetupIndexMap();
	RemoveEditorDataForBody(BoneName);
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

FPhysicsAssetBodyProfileData* UPhysicsAsset::FindMutableBodyProfile(FName BoneName)
{
	const FString Key = MakeBodySetupIndexKey(BoneName);
	for (FPhysicsAssetBodyProfileData& Profile : BodyProfiles)
	{
		if (MakeBodySetupIndexKey(Profile.BoneName) == Key)
		{
			return &Profile;
		}
	}
	return nullptr;
}

const FPhysicsAssetBodyProfileData* UPhysicsAsset::FindBodyProfile(FName BoneName) const
{
	const FString Key = MakeBodySetupIndexKey(BoneName);
	for (const FPhysicsAssetBodyProfileData& Profile : BodyProfiles)
	{
		if (MakeBodySetupIndexKey(Profile.BoneName) == Key)
		{
			return &Profile;
		}
	}
	return nullptr;
}

bool UPhysicsAsset::IsBodyCollisionEnabled(FName BoneName) const
{
	const FPhysicsAssetBodyProfileData* Profile = FindBodyProfile(BoneName);
	return !Profile || Profile->bCollisionEnabled;
}

void UPhysicsAsset::SetBodyCollisionEnabled(FName BoneName, bool bEnabled)
{
	if (!IsValidPhysicsBoneName(BoneName))
	{
		return;
	}

	FPhysicsAssetBodyProfileData* Profile = FindMutableBodyProfile(BoneName);
	if (!Profile)
	{
		BodyProfiles.push_back(FPhysicsAssetBodyProfileData{});
		Profile = &BodyProfiles.back();
		Profile->BoneName = BoneName;
	}
	Profile->bCollisionEnabled = bEnabled;
}

const FString& UPhysicsAsset::GetBodyPhysicalMaterialPath(FName BoneName) const
{
	static const FString NonePath = "None";
	const FPhysicsAssetBodyProfileData* Profile = FindBodyProfile(BoneName);
	return Profile ? Profile->PhysicalMaterialPath : NonePath;
}

void UPhysicsAsset::SetBodyPhysicalMaterialPath(FName BoneName, const FString& InPath)
{
	if (!IsValidPhysicsBoneName(BoneName))
	{
		return;
	}

	FPhysicsAssetBodyProfileData* Profile = FindMutableBodyProfile(BoneName);
	if (!Profile)
	{
		BodyProfiles.push_back(FPhysicsAssetBodyProfileData{});
		Profile = &BodyProfiles.back();
		Profile->BoneName = BoneName;
	}
	Profile->PhysicalMaterialPath = InPath.empty() ? "None" : InPath;
}

bool UPhysicsAsset::IsCollisionDisabled(FName BoneA, FName BoneB) const
{
	if (!IsValidPhysicsBoneName(BoneA) || !IsValidPhysicsBoneName(BoneB) || BoneA == BoneB)
	{
		return false;
	}

	NormalizeBonePair(BoneA, BoneB);
	const FString KeyA = MakeBodySetupIndexKey(BoneA);
	const FString KeyB = MakeBodySetupIndexKey(BoneB);
	for (const FPhysicsAssetCollisionDisablePair& Pair : DisabledCollisionPairs)
	{
		if (MakeBodySetupIndexKey(Pair.BoneA) == KeyA && MakeBodySetupIndexKey(Pair.BoneB) == KeyB)
		{
			return true;
		}
	}
	return false;
}

void UPhysicsAsset::SetCollisionDisabled(FName BoneA, FName BoneB, bool bDisabled)
{
	if (!IsValidPhysicsBoneName(BoneA) || !IsValidPhysicsBoneName(BoneB) || BoneA == BoneB)
	{
		return;
	}

	NormalizeBonePair(BoneA, BoneB);
	const FString KeyA = MakeBodySetupIndexKey(BoneA);
	const FString KeyB = MakeBodySetupIndexKey(BoneB);
	for (auto It = DisabledCollisionPairs.begin(); It != DisabledCollisionPairs.end(); ++It)
	{
		if (MakeBodySetupIndexKey(It->BoneA) == KeyA && MakeBodySetupIndexKey(It->BoneB) == KeyB)
		{
			if (!bDisabled)
			{
				DisabledCollisionPairs.erase(It);
			}
			return;
		}
	}

	if (bDisabled)
	{
		DisabledCollisionPairs.push_back(FPhysicsAssetCollisionDisablePair{ BoneA, BoneB });
	}
}

bool UPhysicsAsset::GetGraphNodePosition(FName BoneName, float& OutX, float& OutY) const
{
	const FString Key = MakeBodySetupIndexKey(BoneName);
	for (const FPhysicsAssetGraphNodePosition& Position : GraphNodePositions)
	{
		if (MakeBodySetupIndexKey(Position.BoneName) == Key)
		{
			OutX = Position.X;
			OutY = Position.Y;
			return true;
		}
	}
	return false;
}

void UPhysicsAsset::SetGraphNodePosition(FName BoneName, float X, float Y)
{
	if (!IsValidPhysicsBoneName(BoneName))
	{
		return;
	}

	const FString Key = MakeBodySetupIndexKey(BoneName);
	for (FPhysicsAssetGraphNodePosition& Position : GraphNodePositions)
	{
		if (MakeBodySetupIndexKey(Position.BoneName) == Key)
		{
			Position.X = X;
			Position.Y = Y;
			return;
		}
	}

	GraphNodePositions.push_back(FPhysicsAssetGraphNodePosition{ BoneName, X, Y });
}

void UPhysicsAsset::RemoveEditorDataForBody(FName BoneName)
{
	if (!IsValidPhysicsBoneName(BoneName))
	{
		return;
	}

	const FString Key = MakeBodySetupIndexKey(BoneName);
	for (auto It = BodyProfiles.begin(); It != BodyProfiles.end();)
	{
		if (MakeBodySetupIndexKey(It->BoneName) == Key)
		{
			It = BodyProfiles.erase(It);
		}
		else
		{
			++It;
		}
	}

	for (auto It = DisabledCollisionPairs.begin(); It != DisabledCollisionPairs.end();)
	{
		if (MakeBodySetupIndexKey(It->BoneA) == Key || MakeBodySetupIndexKey(It->BoneB) == Key)
		{
			It = DisabledCollisionPairs.erase(It);
		}
		else
		{
			++It;
		}
	}

	for (auto It = GraphNodePositions.begin(); It != GraphNodePositions.end();)
	{
		if (MakeBodySetupIndexKey(It->BoneName) == Key)
		{
			It = GraphNodePositions.erase(It);
		}
		else
		{
			++It;
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
	BodyProfiles.clear();
	DisabledCollisionPairs.clear();
	GraphNodePositions.clear();
	BodySetupIndexMap.clear();
}
