#pragma once 
#include "Engine/Object/Object.h"

UENUM(BlueprintType)
enum class ECollisionTraceFlag : uint8
{
	CTF_UseDefault,
	CTF_UseSimpleAndComplex,
	CTF_UseSimpleAsComplex,
	CTF_UseComplexAsSimple,
	CTF_MAX,
};

UENUM()
enum class EPhysicsType : uint8
{
	PhysType_Default,
	PhysType_Kinematic,
	PhysType_Simulated
};

UENUM()
enum class EBodyCollisionResponse : uint8
{
	BodyCollision_Enabled,
	BodyCollision_Disabled,
};

class UBodySetupCore : public UObject
{
public:
	ECollisionTraceFlag GetCollisionTraceFlag() const;

	FName BoneName;
	EPhysicsType PhysicsType;
	ECollisionTraceFlag CollisionTraceFlag;
	EBodyCollisionResponse CollisionReponse;
};
