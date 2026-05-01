#include "Physics/PhysXPhysicsScene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Core/Log.h"

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
		const char* severity = "Info";
		if (code == PxErrorCode::eABORT || code == PxErrorCode::eOUT_OF_MEMORY)
			severity = "Fatal";
		else if (code == PxErrorCode::eINTERNAL_ERROR || code == PxErrorCode::eINVALID_OPERATION)
			severity = "Error";
		else if (code == PxErrorCode::eINVALID_PARAMETER || code == PxErrorCode::ePERF_WARNING)
			severity = "Warning";
		else if (code == PxErrorCode::eDEBUG_WARNING)
			severity = "Warning";

		UE_LOG("[PhysX %s] %s (%s:%d)", severity, message, file, line);
	}
};

static FPhysXErrorCallback GPhysXErrorCallback;
static PxDefaultAllocator GPhysXAllocator;

// ============================================================
// PhysX Foundation/Physics 싱글턴
// PxCreateFoundation은 프로세스당 1회만 허용 — 복수 Scene에서 공유
// ============================================================
static PxFoundation* GSharedFoundation = nullptr;
static PxPhysics* GSharedPhysics = nullptr;
static int32 GSharedRefCount = 0;

static void AcquireSharedPhysX(PxFoundation*& OutFoundation, PxPhysics*& OutPhysics)
{
	if (GSharedRefCount == 0)
	{
		GSharedFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
		if (GSharedFoundation)
		{
			GSharedPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *GSharedFoundation, PxTolerancesScale());
		}
	}
	++GSharedRefCount;
	OutFoundation = GSharedFoundation;
	OutPhysics = GSharedPhysics;
}

static void ReleaseSharedPhysX()
{
	if (--GSharedRefCount <= 0)
	{
		if (GSharedPhysics) { GSharedPhysics->release(); GSharedPhysics = nullptr; }
		if (GSharedFoundation) { GSharedFoundation->release(); GSharedFoundation = nullptr; }
		GSharedRefCount = 0;
	}
}

// ============================================================
// PhysX Simulation Event Callback
// ============================================================
class FPhysXSimulationCallback : public PxSimulationEventCallback
{
public:
	// Block 접촉 → NotifyComponentHit
	void onContact(const PxContactPairHeader& PairHeader,
		const PxContactPair* Pairs, PxU32 Count) override
	{
		if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0
			|| PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
			return;

		auto* CompA = static_cast<UPrimitiveComponent*>(PairHeader.actors[0]->userData);
		auto* CompB = static_cast<UPrimitiveComponent*>(PairHeader.actors[1]->userData);
		if (!CompA || !CompB) return;

		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxContactPair& CP = Pairs[i];
			if (!(CP.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)) continue;

			// Contact point 추출
			PxContactPairPoint ContactPoints[1];
			PxU32 NumPoints = CP.extractContacts(ContactPoints, 1);

			FVector ContactPos(0, 0, 0);
			FVector ContactNormal(0, 0, 1);
			float Penetration = 0.0f;

			if (NumPoints > 0)
			{
				ContactPos = FVector(ContactPoints[0].position.x, ContactPoints[0].position.y, ContactPoints[0].position.z);
				ContactNormal = FVector(ContactPoints[0].normal.x, ContactPoints[0].normal.y, ContactPoints[0].normal.z);
				Penetration = ContactPoints[0].separation; // 음수 = 관통
			}

			FVector NormalImpulse = ContactNormal * Penetration;

			// A → Hit 통지
			FHitResult HitA;
			HitA.bHit = true;
			HitA.HitComponent = CompB;
			HitA.HitActor = CompB->GetOwner();
			HitA.WorldHitLocation = ContactPos;
			HitA.ImpactNormal = ContactNormal;
			HitA.WorldNormal = ContactNormal;
			HitA.PenetrationDepth = -Penetration;
			CompA->NotifyComponentHit(CompA, CompB->GetOwner(), CompB, NormalImpulse, HitA);

			// B → Hit 통지 (법선 반전)
			FHitResult HitB;
			HitB.bHit = true;
			HitB.HitComponent = CompA;
			HitB.HitActor = CompA->GetOwner();
			HitB.WorldHitLocation = ContactPos;
			HitB.ImpactNormal = ContactNormal * -1.0f;
			HitB.WorldNormal = ContactNormal * -1.0f;
			HitB.PenetrationDepth = -Penetration;
			CompB->NotifyComponentHit(CompB, CompA->GetOwner(), CompA, NormalImpulse * -1.0f, HitB);
		}
	}

	// Trigger 진입/이탈 → BeginOverlap / EndOverlap
	void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
	{
		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxTriggerPair& TP = Pairs[i];

			if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;

			auto* TriggerComp = static_cast<UPrimitiveComponent*>(TP.triggerActor->userData);
			auto* OtherComp = static_cast<UPrimitiveComponent*>(TP.otherActor->userData);
			if (!TriggerComp || !OtherComp) continue;

			if (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND)
			{
				FHitResult DummyHit;
				if (TriggerComp->GetGenerateOverlapEvents())
					TriggerComp->NotifyComponentBeginOverlap(TriggerComp, OtherComp->GetOwner(), OtherComp, 0, false, DummyHit);
				if (OtherComp->GetGenerateOverlapEvents())
					OtherComp->NotifyComponentBeginOverlap(OtherComp, TriggerComp->GetOwner(), TriggerComp, 0, false, DummyHit);
			}
			else if (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST)
			{
				if (TriggerComp->GetGenerateOverlapEvents())
					TriggerComp->NotifyComponentEndOverlap(TriggerComp, OtherComp->GetOwner(), OtherComp, 0);
				if (OtherComp->GetGenerateOverlapEvents())
					OtherComp->NotifyComponentEndOverlap(OtherComp, TriggerComp->GetOwner(), TriggerComp, 0);
			}
		}
	}

	void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
	void onWake(PxActor**, PxU32) override {}
	void onSleep(PxActor**, PxU32) override {}
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}
};

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

static FVector ToFVector(const PxVec3& V)
{
	return FVector(V.x, V.y, V.z);
}

static FQuat ToFQuat(const PxQuat& Q)
{
	return FQuat(Q.x, Q.y, Q.z, Q.w);
}

static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
	FVector Pos = Comp->GetWorldLocation();
	FQuat Rot = Comp->GetWorldMatrix().ToQuat();
	return PxTransform(ToPxVec3(Pos), ToPxQuat(Rot));
}

// ============================================================
// Collision Filtering
// ============================================================
// filterData 레이아웃:
//   word0 = 자신의 ObjectType (ECollisionChannel)
//   word1 = Block 비트마스크 (해당 채널에 Block 응답인 비트)
//   word2 = Overlap 비트마스크 (해당 채널에 Overlap 응답인 비트)
//   word3 = 예약

static void SetupFilterData(PxShape* Shape, UPrimitiveComponent* Comp)
{
	PxFilterData Filter;
	Filter.word0 = static_cast<PxU32>(Comp->GetCollisionObjectType());
	Filter.word1 = 0;
	Filter.word2 = 0;
	Filter.word3 = 0;

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		ECollisionResponse R = Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch));
		if (R == ECollisionResponse::Block)   Filter.word1 |= (1u << Ch);
		if (R == ECollisionResponse::Overlap) Filter.word2 |= (1u << Ch);
	}

	Shape->setSimulationFilterData(Filter);
	Shape->setQueryFilterData(Filter);
}

// PxFilterShader — 엔진의 채널/응답 매트릭스를 PhysX에서 처리
// 양쪽 모두 상대 채널에 대해 Block이면 물리 충돌, 한쪽이라도 Overlap이면 트리거, 그 외 무시
static PxFilterFlags KraftonFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* /*constantBlock*/, PxU32 /*constantBlockSize*/)
{
	// 트리거 처리 — 한쪽이라도 트리거면 오버랩 통지만
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	PxU32 channelA = filterData0.word0; // A의 ObjectType
	PxU32 channelB = filterData1.word0; // B의 ObjectType

	// A가 B의 채널에 대해 Block인지, B가 A의 채널에 대해 Block인지
	bool bABlocksB = (filterData0.word1 & (1u << channelB)) != 0;
	bool bBBlocksA = (filterData1.word1 & (1u << channelA)) != 0;

	// 양쪽 모두 Block → 물리 충돌 + contact 콜백
	if (bABlocksB && bBBlocksA)
	{
		pairFlags = PxPairFlag::eCONTACT_DEFAULT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_CONTACT_POINTS;
		return PxFilterFlag::eDEFAULT;
	}

	// 한쪽이라도 Overlap → 겹침 감지만 (물리적 밀어내기 없음)
	bool bAOverlapsB = (filterData0.word2 & (1u << channelB)) != 0;
	bool bBOverlapsA = (filterData1.word2 & (1u << channelA)) != 0;

	if (bAOverlapsB || bBOverlapsA)
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	// Ignore — 쌍 완전히 제거
	return PxFilterFlag::eKILL;
}

// ============================================================
// Lifecycle
// ============================================================

void FPhysXPhysicsScene::Initialize(UWorld* InWorld)
{
	World = InWorld;

	// Foundation / Physics — 프로세스 싱글턴 공유
	AcquireSharedPhysX(Foundation, Physics);
	if (!Foundation || !Physics)
	{
		UE_LOG("[PhysX] Failed to create Foundation or Physics");
		return;
	}

	// CPU Dispatcher
	Dispatcher = PxDefaultCpuDispatcherCreate(2);

	// Event callback
	EventCallback = new FPhysXSimulationCallback();

	// Scene
	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Z-up, m 단위
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = KraftonFilterShader;
	SceneDesc.simulationEventCallback = EventCallback;
	Scene = Physics->createScene(SceneDesc);

	if (!Scene)
	{
		UE_LOG("[PhysX] Failed to create Scene");
		return;
	}

	// Default material (static friction, dynamic friction, restitution)
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.3f);

	UE_LOG("[PhysX] Initialized successfully (Scene=%p)", Scene);
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
	if (EventCallback) { delete EventCallback; EventCallback = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }

	// Foundation/Physics는 공유 싱글턴 — release 카운트 감소만
	Foundation = nullptr;
	Physics = nullptr;
	ReleaseSharedPhysX();

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

	// ── Pre-simulate: Engine → PhysX Transform 동기화 ──
	for (auto& Mapping : BodyMappings)
	{
		if (!Mapping.Component || !Mapping.Actor) continue;

		PxTransform NewPose = GetPxTransform(Mapping.Component);

		if (Mapping.Actor->is<PxRigidDynamic>())
		{
			PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>();
			// Kinematic이면 target으로, 아니면 직접 pose 설정은 하지 않음
			// (Dynamic은 PhysX가 시뮬레이션으로 이동시킴)
			if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
			{
				Dynamic->setKinematicTarget(NewPose);
			}
		}
		else if (Mapping.Actor->is<PxRigidStatic>())
		{
			// Static body — 에디터에서 움직인 경우 위치 갱신
			Mapping.Actor->setGlobalPose(NewPose);
		}
	}

	// ── Simulate ──
	Scene->simulate(DeltaTime);
	Scene->fetchResults(true);

	// ── Post-simulate: PhysX → Engine Transform 동기화 ──
	for (auto& Mapping : BodyMappings)
	{
		if (!Mapping.Component || !Mapping.Actor) continue;

		PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>();
		if (!Dynamic) continue;
		if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) continue;
		if (Dynamic->isSleeping()) continue;

		PxTransform Pose = Dynamic->getGlobalPose();
		FVector NewPos = ToFVector(Pose.p);
		FQuat NewRot = ToFQuat(Pose.q);

		// Root component에 월드 Transform 적용
		UPrimitiveComponent* Comp = Mapping.Component;
		Comp->SetWorldLocation(NewPos);
		Comp->SetRelativeRotation(NewRot);
	}
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

	bool bDynamic = Comp->GetSimulatePhysics();
	if (bDynamic)
	{
		PxRigidDynamic* Dynamic = Physics->createRigidDynamic(BodyTransform);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Dynamic, Geom.any(), *DefaultMaterial);
		if (Shape)
		{
			Shape->setLocalPose(PxTransform(ShapeLocalRot));
			SetupFilterData(Shape, Comp);
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
			SetupFilterData(Shape, Comp);
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
