#include "Physics/BodySetup.h"
#include "Serialization/Archive.h"

void UBodySetup::CreateDefaultBox(const FVector& Center, const FVector& Extents)
{
	AggGeom.BoxElems.clear();

	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = FQuat::Identity;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::Serialize(FArchive& Ar)
{
	Ar << BoneName;
	Ar << AggGeom;
}