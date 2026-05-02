#pragma once

#include "Physics/IPhysicsScene.h"
#include "Core/CoreTypes.h"
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
	class PxRigidActor;
	class PxShape;
}

class FPhysXSimulationCallback;

// ============================================================
// FPhysXPhysicsScene — PhysX 4.1 기반 물리 시스템
//
// IPhysicsScene 인터페이스를 통해 Native와 교체 가능.
//
// 등록 단위는 Actor — 한 액터의 여러 PrimitiveComponent는 하나의
// PxRigidActor에 compound shape로 합쳐진다. 각 shape의 LocalPose는
// 액터 RootComponent에 대한 상대 transform. 이로써 차체 Box + 바퀴
// Sphere 4개처럼 다중 콜라이더가 자연스럽게 한 강체로 동작한다.
// ============================================================
class FPhysXPhysicsScene : public IPhysicsScene
{
public:
	void Initialize(UWorld* InWorld) override;
	void Shutdown() override;

	void RegisterComponent(UPrimitiveComponent* Comp) override;
	void UnregisterComponent(UPrimitiveComponent* Comp) override;
	void RebuildBody(UPrimitiveComponent* Comp) override;

	void Tick(float DeltaTime) override;

	void AddForce(UPrimitiveComponent* Comp, const FVector& Force) override;
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

private:
	UWorld* World = nullptr;

	// PhysX core objects
	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
	FPhysXSimulationCallback* EventCallback = nullptr;

	// Actor 단위 매핑 — 한 액터의 여러 컴포넌트가 같은 PxRigidActor에 shape로 합쳐진다.
	struct FBodyMapping
	{
		AActor* OwnerActor = nullptr;            // 키
		physx::PxRigidActor* Actor = nullptr;    // PhysX rigid (Dynamic/Static)
		UPrimitiveComponent* RootComp = nullptr; // 트랜스폼 동기화 기준 (Actor->RootComponent)
		TArray<UPrimitiveComponent*> Components; // 등록된 컴포넌트들 (shape 1:1 매칭)
	};
	std::vector<FBodyMapping> BodyMappings;

	// 내부 헬퍼
	FBodyMapping* FindMappingByActor(AActor* OwnerActor);
	const FBodyMapping* FindMappingByActor(AActor* OwnerActor) const;
	FBodyMapping* FindMappingByComponent(UPrimitiveComponent* Comp);
	const FBodyMapping* FindMappingByComponent(UPrimitiveComponent* Comp) const;

	// Comp의 geometry를 Mapping의 PxRigidActor에 shape로 추가. 실패 시 nullptr.
	physx::PxShape* AddShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp);
	// Mapping의 actor에서 Comp에 매칭된 shape를 detach.
	void DetachShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp);
};
