#pragma once
#include "BodySetupCore.h"
#include "AggregateGeom.h"
#include "BodySetupPhysicsInfo.h"

#include "Source/Engine/PhysicsEngine/BodySetup.generated.h"

UCLASS()
class UBodySetup : public UBodySetupCore
{
public:
	GENERATED_BODY()

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
	bool ShouldUseMeshTriangleCollision() const { return bUseMeshTriangleCollision; }

	float GetScaledVolume(const FVector& Scale3D = FVector::OneVector) const;
	float CalculateMass(const FVector& Scale3D = FVector::OneVector) const;

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	// DisplayName = Primitives
	UPROPERTY(Edit, Save, Category="Body Shape", DisplayName="Aggregate Geometry", Type=Struct)
	FKAggregateGeom AggGeom;

	UPROPERTY(Edit, Save, Category="Physics Info", DisplayName="Body Physics", Type=Struct)
	FBodySetupPhysicsInfo PhysicsInfo;

	UPROPERTY(Edit, Save, Category="Body Shape", DisplayName="Use Mesh Triangle Collision")
	bool bUseMeshTriangleCollision = false;
};
