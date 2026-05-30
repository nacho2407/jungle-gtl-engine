#include "BodySetup.h"

#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr uint32 BodySetupPhysicsInfoMagic = 0x59485042; // B P H Y
constexpr uint32 BodySetupPhysicsInfoVersion = 1;
constexpr float DefaultBodyDensityKgPerCubicUnit = 0.001f;
constexpr float DefaultRaiseMassToPower = 0.75f;
constexpr float MinBodyMassKg = 0.001f;

void SerializeShapeCommon(FArchive& Ar, FKShapeElem& Elem)
{
	Ar << Elem.RestOffset;

	FName ShapeName = Ar.IsSaving() ? Elem.GetName() : FName();
	Ar << ShapeName;
	if (Ar.IsLoading())
	{
		Elem.SetName(ShapeName);
	}

	bool bContributeToMass = Elem.GetContributeToMass();
	Ar << bContributeToMass;
	if (Ar.IsLoading())
	{
		Elem.SetContributeToMass(bContributeToMass);
	}

	uint8 CollisionEnabled = static_cast<uint8>(Elem.GetCollisionEnabled());
	Ar << CollisionEnabled;
	if (Ar.IsLoading())
	{
		Elem.SetCollisionEnabled(static_cast<ECollisionEnabled>(CollisionEnabled));
	}
}

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

void SerializeBoundingBox(FArchive& Ar, FBoundingBox& Bounds)
{
	Ar << Bounds.Min;
	Ar << Bounds.Max;
}

void SerializeSphereElem(FArchive& Ar, FKSphereElem& Elem)
{
	SerializeShapeCommon(Ar, Elem);
	Ar << Elem.Center;
	Ar << Elem.Radius;
}

void SerializeBoxElem(FArchive& Ar, FKBoxElem& Elem)
{
	SerializeShapeCommon(Ar, Elem);
	Ar << Elem.Center;
	Ar << Elem.Rotation;
	Ar << Elem.X;
	Ar << Elem.Y;
	Ar << Elem.Z;
}

void SerializeSphylElem(FArchive& Ar, FKSphylElem& Elem)
{
	SerializeShapeCommon(Ar, Elem);
	Ar << Elem.Center;
	Ar << Elem.Rotation;
	Ar << Elem.Radius;
	Ar << Elem.Length;
}

void SerializeConvexElem(FArchive& Ar, FKConvexElem& Elem)
{
	SerializeShapeCommon(Ar, Elem);
	Ar << Elem.VertexData;
	Ar << Elem.IndexData;
	SerializeBoundingBox(Ar, Elem.ElemBox);

	FTransform Transform = Elem.GetTransform();
	SerializeTransform(Ar, Transform);
	if (Ar.IsLoading())
	{
		Elem.SetTransform(Transform);
	}
}

template <typename ElemType, typename SerializeFunc>
void SerializeElemArray(FArchive& Ar, TArray<ElemType>& Elems, SerializeFunc&& Func)
{
	uint32 Count = Ar.IsSaving() ? static_cast<uint32>(Elems.size()) : 0;
	Ar << Count;

	if (Ar.IsLoading())
	{
		Elems.clear();
		Elems.resize(Count);
	}

	for (ElemType& Elem : Elems)
	{
		Func(Ar, Elem);
	}
}

void SerializeAggregateGeom(FArchive& Ar, FKAggregateGeom& AggGeom)
{
	SerializeElemArray(Ar, AggGeom.SphereElems, SerializeSphereElem);
	SerializeElemArray(Ar, AggGeom.BoxElems, SerializeBoxElem);
	SerializeElemArray(Ar, AggGeom.SphylElems, SerializeSphylElem);
	SerializeElemArray(Ar, AggGeom.ConvexElems, SerializeConvexElem);
}

void SerializePhysicsInfoPayload(FArchive& Ar, FBodySetupPhysicsInfo& PhysicsInfo)
{
	Ar << PhysicsInfo.bOverrideMass;
	Ar << PhysicsInfo.MassInKgOverride;
	Ar << PhysicsInfo.MassScale;
	Ar << PhysicsInfo.CenterOfMassOffset;
	Ar << PhysicsInfo.LinearDamping;
	Ar << PhysicsInfo.AngularDamping;
	Ar << PhysicsInfo.bEnableGravity;
	Ar << PhysicsInfo.InertiaTensorScale;
}

void SerializePhysicsInfoBlock(FArchive& Ar, FBodySetupPhysicsInfo& PhysicsInfo)
{
	if (Ar.IsSaving())
	{
		uint32 Magic = BodySetupPhysicsInfoMagic;
		uint32 Version = BodySetupPhysicsInfoVersion;
		Ar << Magic;
		Ar << Version;
		SerializePhysicsInfoPayload(Ar, PhysicsInfo);
		return;
	}

	if (!Ar.CanSeek() || Ar.IsAtEnd())
	{
		return;
	}

	const int64 BlockStart = Ar.Tell();
	uint32 Magic = 0;
	Ar << Magic;
	if (Magic != BodySetupPhysicsInfoMagic)
	{
		Ar.Seek(BlockStart);
		return;
	}

	uint32 Version = 0;
	Ar << Version;
	if (Version != BodySetupPhysicsInfoVersion)
	{
		Ar.Seek(BlockStart);
		return;
	}

	SerializePhysicsInfoPayload(Ar, PhysicsInfo);
}
}

void UBodySetup::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << BoneName;

	uint8 PhysicsTypeRaw = static_cast<uint8>(PhysicsType);
	uint8 CollisionTraceFlagRaw = static_cast<uint8>(CollisionTraceFlag);
	uint8 CollisionResponseRaw = static_cast<uint8>(CollisionReponse);

	Ar << PhysicsTypeRaw;
	Ar << CollisionTraceFlagRaw;
	Ar << CollisionResponseRaw;

	if (Ar.IsLoading())
	{
		PhysicsType = static_cast<EPhysicsType>(PhysicsTypeRaw);
		CollisionTraceFlag = static_cast<ECollisionTraceFlag>(CollisionTraceFlagRaw);
		CollisionReponse = static_cast<EBodyCollisionResponse>(CollisionResponseRaw);
	}

	SerializeAggregateGeom(Ar, AggGeom);
	SerializePhysicsInfoBlock(Ar, PhysicsInfo);
}

float UBodySetup::GetScaledVolume(const FVector& Scale3D) const
{
	return AggGeom.GetScaledVolume(Scale3D);
}

float UBodySetup::CalculateMass(const FVector& Scale3D) const
{
	if (PhysicsInfo.bOverrideMass)
	{
		return std::max(PhysicsInfo.MassInKgOverride, MinBodyMassKg);
	}

	const float Volume = GetScaledVolume(Scale3D);
	const float RawMass = std::max(Volume * DefaultBodyDensityKgPerCubicUnit, MinBodyMassKg);
	const float RaisedMass = std::pow(RawMass, DefaultRaiseMassToPower);
	return std::max(RaisedMass * PhysicsInfo.MassScale, MinBodyMassKg);
}
