#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "Physics/PhysXInclude.h"

class AActor;
class UPrimitiveComponent;

enum class EBodyInstanceMode : uint8
{
	Static,
	Dynamic,
	Kinematic
};

struct FBodyInstance
{
	UPrimitiveComponent* OwnerComponent = nullptr;
	physx::PxRigidActor* Body = nullptr;
	EBodyInstanceMode Mode = EBodyInstanceMode::Static;

	bool IsValidBody() const { return Body != nullptr; }
	bool IsDynamic() const;

	void SetGravityEnabled(bool bEnabled);
	void SetKinematic(bool bEnable);

	void SetMass(float InMass);
	float GetMass() const;

	void SetLinearDamping(float InDamping);
	void SetAngularDamping(float InDamping);

	FTransform GetBodyTransform() const;
	void SetBodyTransform(const FTransform& Transform);

	void AddForce(const FVector& Force);
	void AddImpulse(const FVector& Impulse);
	
	void SetLinearVelocity(const FVector& Velocity);
	FVector GetLinearVelocity() const;

	AActor* GetOwnerActor() const;
	void SyncFromPhysics();
	void SyncToPhysics();

	void UpdateFilterData();
};