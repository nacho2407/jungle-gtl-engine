#include "PhysicsAsset.h"

#include "Object/GarbageCollection.h"
#include "Serialization/Archive.h"

namespace
{
	constexpr uint32 ConstraintInitDescMagic = 0x43444943; // C I D C
	constexpr uint32 ConstraintInitDescVersion = 2;

	void SerializeTransform(FArchive& Ar, FTransform& Transform)
	{
		FVector Location = Transform.Location;
		FVector Scale = Transform.Scale;
		float RotationX = Transform.Rotation.X;
		float RotationY = Transform.Rotation.Y;
		float RotationZ = Transform.Rotation.Z;
		float RotationW = Transform.Rotation.W;

		Ar << Location;
		Ar << RotationX;
		Ar << RotationY;
		Ar << RotationZ;
		Ar << RotationW;
		Ar << Scale;

		if (Ar.IsLoading())
		{
			Transform = FTransform(Location, FQuat(RotationX, RotationY, RotationZ, RotationW), Scale);
		}
	}

	void SerializeBodySetups(FArchive& Ar, UPhysicsAsset* PhysicsAsset, TArray<UBodySetup*>& BodySetups)
	{
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
				BodySetups[Index] = UObjectManager::Get().CreateObject<UBodySetup>(PhysicsAsset);
			}

			if (BodySetups[Index])
			{
				BodySetups[Index]->Serialize(Ar);
			}
		}
	}

	void SerializeConstraintInitDesc(FArchive& Ar, FConstraintInstanceInitDesc& Desc, uint32 Version)
	{
		Ar << Desc.ParentBoneName;
		Ar << Desc.ChildBoneName;
		SerializeTransform(Ar, Desc.ParentFrame);
		SerializeTransform(Ar, Desc.ChildFrame);
		Ar << Desc.TwistLimitDegrees;
		Ar << Desc.Swing1LimitDegrees;
		Ar << Desc.Swing2LimitDegrees;
		Ar << Desc.bEnableCollision;

		if (Ar.IsSaving() || Version >= 2)
		{
			Ar << Desc.bEnableProjection;
			Ar << Desc.ProjectionLinearTolerance;
			Ar << Desc.ProjectionAngularToleranceDegrees;
		}
		else if (Ar.IsLoading())
		{
			Desc.bEnableProjection = true;
			Desc.ProjectionLinearTolerance = 10.0f;
			Desc.ProjectionAngularToleranceDegrees = 30.0f;
		}

		if (Ar.IsLoading())
		{
			Desc.ParentBody = nullptr;
			Desc.ChildBody = nullptr;
		}
	}

	void SerializeConstraintInitDescs(FArchive& Ar, TArray<FConstraintInstanceInitDesc>& ConstraintInitDescs)
	{
		if (Ar.IsSaving())
		{
			uint32 Magic = ConstraintInitDescMagic;
			uint32 Version = ConstraintInitDescVersion;
			uint32 Count = static_cast<uint32>(ConstraintInitDescs.size());

			Ar << Magic;
			Ar << Version;
			Ar << Count;

			for (FConstraintInstanceInitDesc& Desc : ConstraintInitDescs)
			{
				Desc.ParentBody = nullptr;
				Desc.ChildBody = nullptr;
				SerializeConstraintInitDesc(Ar, Desc, Version);
			}
			return;
		}

		ConstraintInitDescs.clear();
		if (Ar.IsAtEnd())
		{
			return;
		}

		uint32 Magic = 0;
		Ar << Magic;
		if (Magic != ConstraintInitDescMagic)
		{
			return;
		}

		uint32 Version = 0;
		Ar << Version;
		if (Version > ConstraintInitDescVersion)
		{
			return;
		}

		uint32 Count = 0;
		Ar << Count;
		ConstraintInitDescs.resize(Count);

		for (FConstraintInstanceInitDesc& Desc : ConstraintInitDescs)
		{
			SerializeConstraintInitDesc(Ar, Desc, Version);
		}
	}
}

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

const FConstraintInstanceInitDesc* UPhysicsAsset::FindConstraintInitDescByChildBoneName(const FName& ChildBoneName) const
{
	for (const FConstraintInstanceInitDesc& Desc : ConstraintInitDescs)
	{
		if (Desc.ChildBoneName == ChildBoneName)
		{
			return &Desc;
		}
	}

	return nullptr;
}

FConstraintInstanceInitDesc* UPhysicsAsset::FindConstraintInitDescByChildBoneName(const FName& ChildBoneName)
{
	for (FConstraintInstanceInitDesc& Desc : ConstraintInitDescs)
	{
		if (Desc.ChildBoneName == ChildBoneName)
		{
			return &Desc;
		}
	}

	return nullptr;
}

void UPhysicsAsset::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << SourceSkeletalMeshPath;
	SerializeBodySetups(Ar, this, BodySetups);
	SerializeConstraintInitDescs(Ar, ConstraintInitDescs);
}

void UPhysicsAsset::SerializeLegacyEmbedded(FArchive& Ar, uint32 SerializedObjectNameLength)
{
	if (!Ar.IsLoading())
	{
		return;
	}

	if (SerializedObjectNameLength > 0)
	{
		FString IgnoredName;
		IgnoredName.resize(SerializedObjectNameLength);
		Ar.Serialize(IgnoredName.data(), SerializedObjectNameLength * sizeof(char));
	}

	SerializeBodySetups(Ar, this, BodySetups);
	SerializeConstraintInitDescs(Ar, ConstraintInitDescs);
}

void UPhysicsAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	for (UBodySetup* BodySetup : BodySetups)
	{
		Collector.AddReferencedObject(BodySetup, "UPhysicsAsset.BodySetups");
	}
}
