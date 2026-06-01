#include "Physics/BodySetup.h"
#include "Serialization/Archive.h"

void UBodySetup::Serialize(FArchive& Ar)
{
	Ar << BoneName;
	Ar << AggGeom;
}

void UBodySetup::CreateDefaultBox(const FVector& Center, const FVector& Extents)
{
	AggGeom.BoxElems.clear();

	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = FQuat::Identity;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphere(const FVector& Center, float Radius)
{
	FKSphereElem Sphere;
	Sphere.Center = Center;
	Sphere.Radius = Radius;

	AggGeom.SphereElems.push_back(Sphere);
}

void UBodySetup::AddBox(const FVector& Center, const FQuat& Rotation, const FVector& Extents)
{
	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = Rotation;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphyl(const FVector& Center, const FQuat& Rotation, float Radius, float Length)
{
	FKSphylElem Sphyl;
	Sphyl.Center = Center;
	Sphyl.Rotation = Rotation;
	Sphyl.Radius = Radius;
	Sphyl.Length = Length;
	
	AggGeom.SphylElems.push_back(Sphyl);
}

bool UBodySetup::SetSphereElem(int32 Index, const FVector& Center, float Radius)
{
	if (Index < 0 || Index >= static_cast<int32>(AggGeom.SphereElems.size()))
	{
		return false;
	}

	if (Radius <= 0.0f)
	{
		return false;
	}

	FKSphereElem& Sphere = AggGeom.SphereElems[Index];
	Sphere.Center = Center;
	Sphere.Radius = Radius;
	return true;
}

bool UBodySetup::SetBoxElem(int32 Index, const FVector& Center, const FQuat& Rotation, const FVector& Extents)
{
	if (Index < 0 || Index >= static_cast<int32>(AggGeom.BoxElems.size()))
	{
		return false;
	}

	if (Extents.X <= 0.0f || Extents.Y <= 0.0f || Extents.Z <= 0.0f)
	{
		return false;
	}

	FKBoxElem& Box = AggGeom.BoxElems[Index];
	Box.Center = Center;
	Box.Rotation = Rotation;
	Box.Extents = Extents;
	return true;
}

bool UBodySetup::SetSphylElem(int32 Index, const FVector& Center, const FQuat& Rotation, float Radius, float Length)
{
	if (Index < 0 || Index >= static_cast<int32>(AggGeom.SphylElems.size()))
	{
		return false;
	}
	
	if (Radius <= 0.0f || Length <= 0.0f)
	{
		return false;
	}
	
	FKSphylElem& Sphyl = AggGeom.SphylElems[Index];
	Sphyl.Center = Center;
	Sphyl.Rotation = Rotation;
	Sphyl.Radius = Radius;
	Sphyl.Length = Length;
	return true;	
}

bool UBodySetup::RemovePrimitive(EPhysicsAssetPrimitiveType PrimitiveType, int32 Index)
{
	switch (PrimitiveType)
	{
	case EPhysicsAssetPrimitiveType::Sphere:
		if (Index < 0 || Index >= static_cast<int32>(AggGeom.SphereElems.size()))
		{
			return false;
		}
		AggGeom.SphereElems.erase(AggGeom.SphereElems.begin() + Index);
		return true;

	case EPhysicsAssetPrimitiveType::Box:
		if (Index < 0 || Index >= static_cast<int32>(AggGeom.BoxElems.size()))
		{
			return false;
		}
		AggGeom.BoxElems.erase(AggGeom.BoxElems.begin() + Index);
		return true;

	case EPhysicsAssetPrimitiveType::Capsule:
		if (Index < 0 || Index >= static_cast<int32>(AggGeom.SphylElems.size()))
		{
			return false;
		}
		AggGeom.SphylElems.erase(AggGeom.SphylElems.begin() + Index);
		return true;
	}

	return false;
}
