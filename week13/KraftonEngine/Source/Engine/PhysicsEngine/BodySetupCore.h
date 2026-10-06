#pragma once 
#include "Engine/Object/Object.h"

#include "Source/Engine/PhysicsEngine/BodySetupCore.generated.h"

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

UCLASS()
class UBodySetupCore : public UObject
{
public:
	GENERATED_BODY()

	ECollisionTraceFlag GetCollisionTraceFlag() const;

	UPROPERTY(Edit, Save, Category="Body Setup", DisplayName="Bone Name")
	FName BoneName;

	UPROPERTY(Edit, Save, Category="Body Setup", DisplayName="Physics Type", Enum=EPhysicsType)
	EPhysicsType PhysicsType;

	UPROPERTY(Edit, Save, Category="Body Setup", DisplayName="Collision Trace Flag", Enum=ECollisionTraceFlag)
	ECollisionTraceFlag CollisionTraceFlag;

	UPROPERTY(Edit, Save, Category="Body Setup", DisplayName="Collision Response", Enum=EBodyCollisionResponse)
	EBodyCollisionResponse CollisionReponse;
};
