#pragma once

#include "Actor.h"


class USkyComponent;
class ENGINE_API ASkySphereActor : public AActor
{
public:
	DECLARE_RTTI(ASkySphereActor, AActor)
	void PostSpawnInitialize() override;


	void Tick(float DeltaTime) override;
	USkyComponent* GetSkyComponent() const { return SkyComponent; }

private:
	USkyComponent* SkyComponent = nullptr;
};
