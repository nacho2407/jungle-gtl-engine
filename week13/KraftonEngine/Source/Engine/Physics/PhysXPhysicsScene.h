#pragma once

#include "Physics/IPhysicsScene.h"
#include "Core/Types/CoreTypes.h"
#include <vector>

class AActor;

// Forward declarations — PhysX types
namespace physx
{
	class PxFoundation;
	class PxPhysics;
	class PxScene;
	class PxDefaultCpuDispatcher;
	class PxMaterial;
	class PxActor;
	class PxRigidActor;
	class PxShape;
}

class FPhysXSimulationCallback;

// ============================================================
// FPhysXPhysicsScene — PhysX 4.1 기반 물리 시스템
//
// IPhysicsScene 인터페이스를 통해 Native와 교체 가능.
//
// 등록 단위는 FBodyInstance.
// 일반 PrimitiveComponent는 자신의 BodyInstance를 통해 PxRigidActor 하나를 가진다.
// SkeletalMesh ragdoll은 Bone별 FBodyInstance를 별도로 생성하고,
// 일반 PrimitiveComponent sync 경로와 분리해서 SkeletalMeshComponent에서 Bone pose로 동기화한다.
// ============================================================
class FPhysXPhysicsScene : public IPhysicsScene
{
public:
	void Initialize(UWorld* InWorld) override;
	void Shutdown() override;

	void RegisterComponent(UPrimitiveComponent* Comp) override;
	void UnregisterComponent(UPrimitiveComponent* Comp) override;
	void RebuildBody(UPrimitiveComponent* Comp) override;

	bool CreateBodyInstance(FBodyInstance& Body, const FBodyInstanceInitDesc& Desc) override;
	void DestroyBodyInstance(FBodyInstance& Body) override;

	bool CreateConstraintInstance(FConstraintInstance& Constraint) override;
	void DestroyConstraintInstance(FConstraintInstance& Constraint) override;

	void Tick(float DeltaTime) override;

	void AddForce(UPrimitiveComponent* Comp, const FVector& Force) override;
	void AddImpulse(UPrimitiveComponent* Comp, const FVector& Impulse) override;
	void AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation) override;
	void AddTorque(UPrimitiveComponent* Comp, const FVector& Torque) override;

	FVector GetLinearVelocity(UPrimitiveComponent* Comp) const override;
	void SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;
	FVector GetAngularVelocity(UPrimitiveComponent* Comp) const override;
	void SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;

	void SetMass(UPrimitiveComponent* Comp, float Mass) override;
	float GetMass(UPrimitiveComponent* Comp) const override;
	void SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset) override;
	FVector GetCenterOfMass(UPrimitiveComponent* Comp) const override;

	bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr) const override;
	bool Sweep(const FVector& Start, const FVector& Dir, float MaxDist, const FCollisionShape& Shape, const FQuat& ShapeRot, FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const override;
	bool RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr) const override;

	physx::PxScene* GetPxScene() const { return Scene; }
	physx::PxMaterial* GetDefaultMaterial() const { return DefaultMaterial; }

private:
	UWorld* World = nullptr;

	// PhysX core objects
	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
	FPhysXSimulationCallback* EventCallback = nullptr;

	std::vector<FBodyInstance*> RegisteredBodies;

	bool bSharedPhysXAcquired = false;
	bool bShutdownComplete = true;

	void AddRegisteredBody(FBodyInstance* Body);
	void RemoveRegisteredBody(FBodyInstance* Body);
	void ReleaseRegisteredBodies();

	FBodyInstance* FindRegisteredBodyByActor(const physx::PxActor* Actor) const;

	void SyncEngineToPhysicsBeforeSim();
	void SimulatePhysics(float DeltaTime);
	void SyncPhysicsToEngineAfterSim();
	void DispatchPhysicsEvents();
};
