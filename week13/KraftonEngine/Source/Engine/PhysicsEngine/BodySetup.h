#pragma once
#include "BodySetupCore.h"
#include "AggregateGeom.h"

class UBodySetup : public UBodySetupCore
{
public:
	UBodySetup()
	{
		PhysicsType = EPhysicsType::PhysType_Default;
		CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
		CollisionReponse = EBodyCollisionResponse::BodyCollision_Enabled;
	}

	const FKAggregateGeom& GetAggGeom() const { return AggGeom; }
	FKAggregateGeom& GetAggGeom() { return AggGeom; }

	void Serialize(FArchive& Ar) override;

private:
	// DisplayName = Primitives
	FKAggregateGeom AggGeom;
};
