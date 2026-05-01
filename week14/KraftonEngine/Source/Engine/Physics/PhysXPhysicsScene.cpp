#include "Physics/PhysXPhysicsScene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"

// PhysX headers
#include <PxPhysicsAPI.h>

using namespace physx;

// ============================================================
// PhysX Error Callback
// ============================================================
class FPhysXErrorCallback : public PxErrorCallback
{
public:
	void reportError(PxErrorCode::Enum code, const char* message,
		const char* file, int line) override
	{
		// TODO: FLog 연동
		(void)code; (void)message; (void)file; (void)line;
	}
};

static FPhysXErrorCallback GPhysXErrorCallback;
static PxDefaultAllocator GPhysXAllocator;

// ============================================================
// Transform 변환 유틸
// ============================================================
static PxVec3 ToPxVec3(const FVector& V)
{
	return PxVec3(V.X, V.Y, V.Z);
}

static PxQuat ToPxQuat(const FQuat& Q)
{
	return PxQuat(Q.X, Q.Y, Q.Z, Q.W);
}

static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
	FVector Pos = Comp->GetWorldLocation();
	FQuat Rot = Comp->GetWorldMatrix().ToQuat();
	return PxTransform(ToPxVec3(Pos), ToPxQuat(Rot));
}

// ============================================================
// Lifecycle
// ============================================================

void FPhysXPhysicsScene::Initialize(UWorld* InWorld)
{
	World = InWorld;

	// Foundation
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
	if (!Foundation) return;

	// Physics
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale());
	if (!Physics) return;

	// CPU Dispatcher
	Dispatcher = PxDefaultCpuDispatcherCreate(2);

	// Scene
	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = PxVec3(0.0f, 0.0f, -981.0f); // Z-up, cm 단위
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = PxDefaultSimulationFilterShader;
	Scene = Physics->createScene(SceneDesc);

	// Default material (static friction, dynamic friction, restitution)
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.3f);
}

void FPhysXPhysicsScene::Shutdown()
{
	// Body 정리
	for (auto& Mapping : BodyMappings)
	{
		if (Mapping.Actor)
		{
			Mapping.Actor->release();
			Mapping.Actor = nullptr;
		}
	}
	BodyMappings.clear();

	if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
	if (Scene) { Scene->release(); Scene = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }
	if (Physics) { Physics->release(); Physics = nullptr; }
	if (Foundation) { Foundation->release(); Foundation = nullptr; }

	World = nullptr;
}

// ============================================================
// Body 관리
// ============================================================

void FPhysXPhysicsScene::RegisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene) return;
	if (FindMapping(Comp)) return; // 이미 등록됨

	PxRigidActor* Body = CreateBodyForComponent(Comp);
	if (!Body) return;

	Scene->addActor(*Body);
	BodyMappings.push_back({ Comp, Body });
}

void FPhysXPhysicsScene::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene) return;

	FBodyMapping* Mapping = FindMapping(Comp);
	if (!Mapping) return;

	if (Mapping->Actor)
	{
		Scene->removeActor(*Mapping->Actor);
		Mapping->Actor->release();
	}

	// swap-and-pop
	*Mapping = BodyMappings.back();
	BodyMappings.pop_back();
}

// ============================================================
// Simulation
// ============================================================

void FPhysXPhysicsScene::Tick(float DeltaTime)
{
	if (!Scene || DeltaTime <= 0.0f) return;

	// TODO: Transform 동기화 (Engine → PhysX)
	// TODO: Simulate
	// Scene->simulate(DeltaTime);
	// Scene->fetchResults(true);
	// TODO: Transform 동기화 (PhysX → Engine)
	// TODO: Collision callback → Overlap/Hit 이벤트 브릿지
}

// ============================================================
// Internal helpers
// ============================================================

PxRigidActor* FPhysXPhysicsScene::CreateBodyForComponent(UPrimitiveComponent* Comp)
{
	if (!Physics || !DefaultMaterial) return nullptr;

	// Shape Component 타입에 따라 PxGeometry 결정
	PxGeometryHolder Geom;
	bool bHasGeom = false;

	// Capsule은 PhysX에서 X축 기준이므로 로컬 회전 보정 필요
	PxQuat ShapeLocalRot = PxQuat(PxIdentity);

	if (auto* Box = Cast<UBoxComponent>(Comp))
	{
		FVector Ext = Box->GetScaledBoxExtent();
		Geom = PxBoxGeometry(Ext.X, Ext.Y, Ext.Z);
		bHasGeom = true;
	}
	else if (auto* Sphere = Cast<USphereComponent>(Comp))
	{
		float Radius = Sphere->GetScaledSphereRadius();
		Geom = PxSphereGeometry(Radius);
		bHasGeom = true;
	}
	else if (auto* Capsule = Cast<UCapsuleComponent>(Comp))
	{
		float Radius = Capsule->GetScaledCapsuleRadius();
		float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		// PhysX Capsule은 X축 기준 → 엔진의 Z축 기준으로 90° 보정
		Geom = PxCapsuleGeometry(Radius, HalfHeight - Radius);
		ShapeLocalRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
		bHasGeom = true;
	}

	if (!bHasGeom) return nullptr;

	// Body 타입 결정
	PxTransform BodyTransform = GetPxTransform(Comp);
	PxRigidActor* Body = nullptr;

	bool bSimulate = (Comp->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	if (bSimulate)
	{
		PxRigidDynamic* Dynamic = Physics->createRigidDynamic(BodyTransform);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Dynamic, Geom.any(), *DefaultMaterial);
		if (Shape)
		{
			Shape->setLocalPose(PxTransform(ShapeLocalRot));
		}
		PxRigidBodyExt::updateMassAndInertia(*Dynamic, 1.0f);
		Body = Dynamic;
	}
	else
	{
		PxRigidStatic* Static = Physics->createRigidStatic(BodyTransform);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Static, Geom.any(), *DefaultMaterial);
		if (Shape)
		{
			Shape->setLocalPose(PxTransform(ShapeLocalRot));
		}
		Body = Static;
	}

	// Overlap 전용인 경우 트리거로 설정
	if (Body && Comp->GetGenerateOverlapEvents())
	{
		PxU32 ShapeCount = Body->getNbShapes();
		PxShape* Shapes[1];
		Body->getShapes(Shapes, 1);
		if (ShapeCount > 0 && Shapes[0])
		{
			Shapes[0]->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			Shapes[0]->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
		}
	}

	// userData로 Component 포인터 저장 — 콜백에서 역참조용
	if (Body)
	{
		Body->userData = Comp;
	}

	return Body;
}

void FPhysXPhysicsScene::RemoveBody(PxRigidActor* Body)
{
	if (Body && Scene)
	{
		Scene->removeActor(*Body);
		Body->release();
	}
}

FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMapping(UPrimitiveComponent* Comp)
{
	for (auto& M : BodyMappings)
	{
		if (M.Component == Comp) return &M;
	}
	return nullptr;
}
