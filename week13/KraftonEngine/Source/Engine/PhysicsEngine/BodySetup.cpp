#include "BodySetup.h"

#include "Serialization/Archive.h"

namespace
{
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
}
