#include "Physics/PhysicalMaterial.h"

#include "Serialization/Archive.h"

void UPhysicalMaterial::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << AssetPathFileName;
	Ar << Friction;
	Ar << Restitution;
	Ar << Density;
	Ar << FrictionCombineMode;
	Ar << RestitutionCombineMode;
}
