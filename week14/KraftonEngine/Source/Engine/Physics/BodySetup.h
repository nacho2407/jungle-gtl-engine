#pragma once

#include "Physics/BodySetupCore.h"
#include "Physics/PhysicsGeometry.h"

#include "Source/Engine/Physics/BodySetup.generated.h"

UCLASS()
class UBodySetup : public UBodySetupCore
{
public:
	GENERATED_BODY()
	
	void Serialize(FArchive& Ar) override; 

	FKAggregateGeom& GetAggGeom() { return AggGeom; }
	const FKAggregateGeom& GetAggGeom() const { return AggGeom; }

	bool HasSimpleCollision() const { return !AggGeom.IsEmpty(); }

	void CreateDefaultBox(const FVector& Center, const FVector& Extents);

	void AddSphere(const FVector& Center, float Radius);
	void AddBox(const FVector& Center, const FQuat& Rotation, const FVector& Extents);
	void AddSphyl(const FVector& Center, const FQuat& Rotation, float Radius, float Length);
	
protected:
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Primitives")
	FKAggregateGeom AggGeom;
};
