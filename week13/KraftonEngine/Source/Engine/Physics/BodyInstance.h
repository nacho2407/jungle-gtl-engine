#pragma once
#include <PxRigidActor.h>

#include "Core/Types/CoreTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Transform.h"
#include "Object/FName.h"

class AActor;
class UPrimitiveComponent;
class USkeletalMeshComponent;

// PhysX Body에 붙일 Shape 종류
enum class EBodyInstanceShapeType : uint8
{
	Sphere,
	Box,
	Capsule,
	Convex,
	TriangleMesh
};

struct FBodyShapeDesc
{
	EBodyInstanceShapeType ShapeType = EBodyInstanceShapeType::Sphere;

	FTransform LocalTransform;

	FVector BoxHalfExtent = FVector(10.0f, 10.0f, 10.0f);
	float SphereRadius = 10.0f;
	float CapsuleRadius = 5.0f;
	float CapsuleHalfHeight = 20.0f;

	TArray<FVector> ConvexVertices;
	TArray<int32> ConvexIndices;
	FVector ConvexScale = FVector::OneVector;

	TArray<FVector> TriangleVertices;
	TArray<uint32> TriangleIndices;
};

// FBodyInscane를 만들 때 필요한 초기화 정보
struct FBodyInstanceInitDesc
{
	FTransform WorldTransform;
	TArray<FBodyShapeDesc> Shapes;

	bool bSimulatePhysics = true;
	bool bKinematic = false;

	ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ECollisionChannel ObjectType = ECollisionChannel::WorldDynamic;
	FCollisionResponseContainer ResponseContainer;
	bool bIgnoreSameOwner = true;

	float Mass = 1.0f;
	FVector CenterOfMassOffset = FVector::ZeroVector;
	float LinearDamping = 0.0f;
	float AngularDamping = 0.0f;
	bool bEnableGravity = true;
	FVector InertiaTensorScale = FVector(1.0f, 1.0f, 1.0f);
};

struct FBodyInstance
{
	UPrimitiveComponent* OwnerComponent = nullptr;
	USkeletalMeshComponent* OwnerSkeletalComponent = nullptr;

	AActor* GetOwnerActor() const;

	FName BoneName = FName::None;
	int32 BoneIndex = -1;

	bool bSimulatePhysics = true;
	bool bKinematic = false;

	ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	ECollisionChannel ObjectType = ECollisionChannel::WorldDynamic;
	FCollisionResponseContainer ResponseContainer;
	bool bIgnoreSameOwner = false;

	float Mass = 1.0f;
	FVector CenterOfMassOffset = FVector::ZeroVector;
	float LinearDamping = 0.0f;
	float AngularDamping = 0.0f;
	bool bEnableGravity = true;
	FVector InertiaTensorScale = FVector(1.0f, 1.0f, 1.0f);

	physx::PxRigidActor* RigidActor = nullptr;
	TArray<physx::PxShape*> Shapes;

	bool IsValidBodyInstance() const { return RigidActor != nullptr; }

	physx::PxRigidDynamic* GetRigidDynamic() const;
	physx::PxRigidStatic* GetRigidStatic() const;

	FTransform GetBodyTransform() const;
	void SetBodyTransform(const FTransform& WorldTransform);
	void SetKinematicTarget(const FTransform& WorldTransform);
	void SetKinematic(bool bInKinematic);
	void SetGravityEnabled(bool bInEnableGravity);

	void AddForce(const FVector& Force);
	void AddImpulse(const FVector& Impulse);
	void WakeUp();
	void SetLinearVelocity(const FVector& Velocity);
	FVector GetLinearVelocity() const;

	void AddForceAtLocation(const FVector& Force, const FVector& WorldLocation);
	void AddTorque(const FVector& Torque);

	void SetAngularVelocity(const FVector& Velocity);
	FVector GetAngularVelocity() const;

	void SetMass(float NewMass);
	float GetMass() const;

	void SetCenterOfMass(const FVector& LocalOffset);
	FVector GetCenterOfMass() const;

	void ClearPhysicsPointers();
};

bool BuildBodyInstanceInitDescFromPrimitive(UPrimitiveComponent* Comp, FBodyInstanceInitDesc& OutDesc);
