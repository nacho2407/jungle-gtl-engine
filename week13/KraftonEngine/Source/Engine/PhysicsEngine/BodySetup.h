#pragma once
#include "BodySetupCore.h"
#include "AggregateGeom.h"
#include "BodySetupPhysicsInfo.h"

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
	const FBodySetupPhysicsInfo& GetPhysicsInfo() const { return PhysicsInfo; }
	FBodySetupPhysicsInfo& GetPhysicsInfo() { return PhysicsInfo; }

	float GetScaledVolume(const FVector& Scale3D = FVector::OneVector) const;
	float CalculateMass(const FVector& Scale3D = FVector::OneVector) const;

	void Serialize(FArchive& Ar) override;

private:
	// DisplayName = Primitives
	FKAggregateGeom AggGeom;
	FBodySetupPhysicsInfo PhysicsInfo;
};
