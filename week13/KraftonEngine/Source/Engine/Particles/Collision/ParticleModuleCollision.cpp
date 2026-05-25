#include "ParticleModuleCollision.h"
#include "Serialization/Archive.h"

void UParticleModuleCollision::Update(const FUpdateContext& Context)
{
}

void UParticleModuleCollision::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Radius;
	Ar << Restitution;
	Ar << bKillOnCollision;
}
