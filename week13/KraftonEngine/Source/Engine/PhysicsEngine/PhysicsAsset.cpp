#include "PhysicsAsset.h"

#include "Object/GarbageCollection.h"
#include "Serialization/Archive.h"

int32 UPhysicsAsset::FindBodyIndexByBoneName(const FName& BoneName) const
{
	for (int32 Index = 0; Index < static_cast<int32>(BodySetups.size()); ++Index)
	{
		const UBodySetup* BodySetup = BodySetups[Index];
		if (BodySetup && BodySetup->BoneName == BoneName)
		{
			return Index;
		}
	}

	return -1;
}

UBodySetup* UPhysicsAsset::FindBodySetupByBoneName(const FName& BoneName) const
{
	const int32 BodyIndex = FindBodyIndexByBoneName(BoneName);
	if (BodyIndex == -1)
	{
		return nullptr;
	}

	return BodySetups[BodyIndex];
}

void UPhysicsAsset::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	uint32 BodySetupCount = Ar.IsSaving() ? static_cast<uint32>(BodySetups.size()) : 0;
	Ar << BodySetupCount;

	if (Ar.IsLoading())
	{
		BodySetups.clear();
		BodySetups.resize(BodySetupCount, nullptr);
	}

	for (uint32 Index = 0; Index < BodySetupCount; ++Index)
	{
		bool bHasBodySetup = Ar.IsSaving() && BodySetups[Index];
		Ar << bHasBodySetup;

		if (!bHasBodySetup)
		{
			if (Ar.IsLoading())
			{
				BodySetups[Index] = nullptr;
			}
			continue;
		}

		if (Ar.IsLoading())
		{
			BodySetups[Index] = UObjectManager::Get().CreateObject<UBodySetup>(this);
		}

		if (BodySetups[Index])
		{
			BodySetups[Index]->Serialize(Ar);
		}
	}
}

void UPhysicsAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	for (UBodySetup* BodySetup : BodySetups)
	{
		Collector.AddReferencedObject(BodySetup, "UPhysicsAsset.BodySetups");
	}
}
