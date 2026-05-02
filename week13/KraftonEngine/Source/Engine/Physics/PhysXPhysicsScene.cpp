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

		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxContactPair& CP = Pairs[i];
			if (!(CP.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)) continue;

			// Compound shape: actor 단위가 아닌 shape별 userData에 PrimitiveComponent 저장.
			auto* CompA = CP.shapes[0] ? static_cast<UPrimitiveComponent*>(CP.shapes[0]->userData) : nullptr;
			auto* CompB = CP.shapes[1] ? static_cast<UPrimitiveComponent*>(CP.shapes[1]->userData) : nullptr;
			if (!CompA || !CompB) continue;

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

			// Compound shape: shape userData가 PrimitiveComponent.
			auto* TriggerComp = TP.triggerShape ? static_cast<UPrimitiveComponent*>(TP.triggerShape->userData) : nullptr;
			auto* OtherComp   = TP.otherShape   ? static_cast<UPrimitiveComponent*>(TP.otherShape->userData)   : nullptr;
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
//   word3 = 소유 액터 UUID — 같은 액터의 두 컴포넌트끼리 충돌을 무시하기 위함
//           (Native 측 O(N²) 루프의 `if (A->GetOwner() == B->GetOwner()) continue;` 가드와 동일 의미)
//           Owner가 없거나 UUID가 0이면 가드 미적용.

static void SetupFilterData(PxShape* Shape, UPrimitiveComponent* Comp)
{
	PxFilterData Filter;
	Filter.word0 = static_cast<PxU32>(Comp->GetCollisionObjectType());
	Filter.word1 = 0;
	Filter.word2 = 0;
	Filter.word3 = Comp->GetOwner() ? Comp->GetOwner()->GetUUID() : 0;

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
	// 같은 액터(같은 owner UUID)의 두 컴포넌트끼리는 충돌 무시.
	// Native 측 O(N²) 루프의 same-owner 가드와 동일 의미. 차량 차체-바퀴처럼
	// 한 액터가 여러 콜라이더를 가질 때 자기끼리 충돌 시뮬레이션되는 문제를 막는다.
	if (filterData0.word3 != 0 && filterData0.word3 == filterData1.word3)
	{
		return PxFilterFlag::eKILL;
	}

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
// Body 관리 — Actor 단위 compound
//
// 한 액터의 여러 PrimitiveComponent는 같은 PxRigidActor에 shape로 합쳐진다.
// shape의 LocalPose는 액터 RootComponent에 대한 상대 transform.
// userData: PxActor → AActor, PxShape → UPrimitiveComponent.
// ============================================================

void FPhysXPhysicsScene::RegisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene || !Physics || !DefaultMaterial) return;
	if (FindMappingByComponent(Comp)) return; // 이미 등록됨

	AActor* OwnerActor = Comp->GetOwner();
	if (!OwnerActor) return;

	FBodyMapping* Mapping = FindMappingByActor(OwnerActor);

	if (!Mapping)
	{
		// 액터의 첫 등록 — PxRigidActor 생성. RootComponent가 PrimitiveComponent면 그걸,
		// 아니면 fallback으로 들어온 Comp를 RootComp로 사용.
		UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		if (!RootPrim) RootPrim = Comp;

		const bool bDynamic = RootPrim->GetSimulatePhysics();
		PxTransform BodyXf = GetPxTransform(RootPrim);

		PxRigidActor* Body = bDynamic
			? static_cast<PxRigidActor*>(Physics->createRigidDynamic(BodyXf))
			: static_cast<PxRigidActor*>(Physics->createRigidStatic(BodyXf));
		if (!Body) return;

		Body->userData = OwnerActor;
		Scene->addActor(*Body);

		FBodyMapping NewMapping;
		NewMapping.OwnerActor = OwnerActor;
		NewMapping.Actor = Body;
		NewMapping.RootComp = RootPrim;
		BodyMappings.push_back(NewMapping);
		Mapping = &BodyMappings.back();
	}

	// shape 추가
	PxShape* Shape = AddShapeForComponent(*Mapping, Comp);
	if (!Shape) return;
	Mapping->Components.push_back(Comp);

	// Dynamic이면 mass/inertia 재계산 (shape 추가 시마다)
	if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
	{
		PxRigidBodyExt::updateMassAndInertia(*Dyn, 1.0f);
	}
}

void FPhysXPhysicsScene::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene) return;

	FBodyMapping* Mapping = FindMappingByComponent(Comp);
	if (!Mapping) return;

	// 해당 컴포넌트의 shape detach
	DetachShapeForComponent(*Mapping, Comp);

	// Components 배열에서 제거
	Mapping->Components.erase(
		std::remove(Mapping->Components.begin(), Mapping->Components.end(), Comp),
		Mapping->Components.end());

	// 마지막 컴포넌트가 빠지면 actor 자체도 release
	if (Mapping->Components.empty())
	{
		if (Mapping->Actor)
		{
			Scene->removeActor(*Mapping->Actor);
			Mapping->Actor->release();
		}

		// swap-and-pop
		*Mapping = BodyMappings.back();
		BodyMappings.pop_back();
		return;
	}

	// 남은 shape가 있으면 mass 재계산
	if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
	{
		PxRigidBodyExt::updateMassAndInertia(*Dyn, 1.0f);
	}
}

// ============================================================
// Simulation
// ============================================================

void FPhysXPhysicsScene::Tick(float DeltaTime)
{
	if (!Scene || DeltaTime <= 0.0f) return;

	// ── Pre-simulate: Engine → PhysX Transform 동기화 ──
	// 한 PxActor가 여러 컴포넌트를 가지므로 RootComp 기준으로만 한 번 동기화.
	for (auto& Mapping : BodyMappings)
	{
		if (!Mapping.RootComp || !Mapping.Actor) continue;

		PxTransform NewPose = GetPxTransform(Mapping.RootComp);

		if (PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>())
		{
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
	// RootComp에만 transform 적용 → 자식 컴포넌트는 attach로 자동 따라감.
	for (auto& Mapping : BodyMappings)
	{
		if (!Mapping.RootComp || !Mapping.Actor) continue;

		PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>();
		if (!Dynamic) continue;
		if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) continue;
		if (Dynamic->isSleeping()) continue;

		PxTransform Pose = Dynamic->getGlobalPose();
		FVector NewPos = ToFVector(Pose.p);
		FQuat NewRot = ToFQuat(Pose.q);

		Mapping.RootComp->SetWorldLocation(NewPos);
		Mapping.RootComp->SetRelativeRotation(NewRot);
	}
}

// ============================================================
// Internal helpers
// ============================================================

PxShape* FPhysXPhysicsScene::AddShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
	if (!Mapping.Actor || !DefaultMaterial || !Comp) return nullptr;

	// Shape Component 타입에 따라 PxGeometry 결정
	PxGeometryHolder Geom;
	bool bHasGeom = false;

	// Capsule은 PhysX에서 X축 기준이므로 로컬 회전 보정 필요
	PxQuat ShapeAxisRot = PxQuat(PxIdentity);

	if (auto* Box = Cast<UBoxComponent>(Comp))
	{
		FVector Ext = Box->GetScaledBoxExtent();
		Geom = PxBoxGeometry(Ext.X, Ext.Y, Ext.Z);
		bHasGeom = true;
	}
	else if (auto* Sphere = Cast<USphereComponent>(Comp))
	{
		Geom = PxSphereGeometry(Sphere->GetScaledSphereRadius());
		bHasGeom = true;
	}
	else if (auto* Capsule = Cast<UCapsuleComponent>(Comp))
	{
		float Radius = Capsule->GetScaledCapsuleRadius();
		float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Geom = PxCapsuleGeometry(Radius, HalfHeight - Radius);
		ShapeAxisRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
		bHasGeom = true;
	}

	if (!bHasGeom) return nullptr;

	PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Mapping.Actor, Geom.any(), *DefaultMaterial);
	if (!Shape) return nullptr;

	// Local pose: Comp의 RootComp 대비 상대 transform.
	// Compound shape에서 자식 컴포넌트가 부모(=PxActor 기준)에 정확히 박혀있도록.
	PxTransform LocalPose = PxTransform(PxIdentity);
	if (Comp != Mapping.RootComp && Mapping.RootComp)
	{
		FVector RootPos = Mapping.RootComp->GetWorldLocation();
		FQuat RootRot = Mapping.RootComp->GetWorldMatrix().ToQuat();
		FVector CompPos = Comp->GetWorldLocation();
		FQuat CompRot = Comp->GetWorldMatrix().ToQuat();

		FQuat InvRootRot = RootRot.Inverse();
		FVector LocalPos = InvRootRot.RotateVector(CompPos - RootPos);
		FQuat LocalRot = InvRootRot * CompRot;

		LocalPose = PxTransform(ToPxVec3(LocalPos), ToPxQuat(LocalRot));
	}

	// Capsule 등 축 보정을 LocalPose의 회전 부분에 합성
	LocalPose.q = LocalPose.q * ShapeAxisRot;
	Shape->setLocalPose(LocalPose);

	SetupFilterData(Shape, Comp);

	// Trigger flag — 같은 PxActor에 simulation shape와 trigger shape가 섞이면
	// PhysX가 actor를 trigger로 동작시키므로 주의. 같은 액터의 모든 shape가
	// 같은 종류여야 안전.
	if (Comp->GetGenerateOverlapEvents())
	{
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	// userData: shape 단위로 PrimitiveComponent 매핑 — 콜백에서 역참조용
	Shape->userData = Comp;

	return Shape;
}

void FPhysXPhysicsScene::DetachShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
	if (!Mapping.Actor || !Comp) return;

	const PxU32 NumShapes = Mapping.Actor->getNbShapes();
	if (NumShapes == 0) return;

	std::vector<PxShape*> Shapes(NumShapes);
	Mapping.Actor->getShapes(Shapes.data(), NumShapes);

	for (PxShape* Shape : Shapes)
	{
		if (Shape && Shape->userData == Comp)
		{
			Mapping.Actor->detachShape(*Shape);
			break;
		}
	}
}

FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor)
{
	for (auto& M : BodyMappings)
	{
		if (M.OwnerActor == OwnerActor) return &M;
	}
	return nullptr;
}

const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor) const
{
	for (const auto& M : BodyMappings)
	{
		if (M.OwnerActor == OwnerActor) return &M;
	}
	return nullptr;
}

// "이 컴포넌트가 shape로 추가된 mapping" 검색 — 등록 가드 + Force/Velocity API 라우팅용.
// owner 기반 lookup과 다름: 같은 owner라도 컴포넌트가 아직 Components에 push되지 않았으면
// 다른 컴포넌트의 shape를 통해 force가 잘못 적용되지 않도록 nullptr 반환.
FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return nullptr;
	for (auto& M : BodyMappings)
	{
		for (UPrimitiveComponent* C : M.Components)
		{
			if (C == Comp) return &M;
		}
	}
	return nullptr;
}

const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp) const
{
	if (!Comp) return nullptr;
	for (const auto& M : BodyMappings)
	{
		for (UPrimitiveComponent* C : M.Components)
		{
			if (C == Comp) return &M;
		}
	}
	return nullptr;
}

// ============================================================
// Force / Torque
// ============================================================

void FPhysXPhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	Dyn->addForce(ToPxVec3(Force));
}

void FPhysXPhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	PxRigidBodyExt::addForceAtPos(*Dyn, ToPxVec3(Force), ToPxVec3(WorldLocation));
}

void FPhysXPhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	Dyn->addTorque(ToPxVec3(Torque));
}

// ============================================================
// Velocity
// ============================================================

FVector FPhysXPhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return { 0, 0, 0 };
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return { 0, 0, 0 };
	return ToFVector(Dyn->getLinearVelocity());
}

void FPhysXPhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	Dyn->setLinearVelocity(ToPxVec3(Vel));
}

FVector FPhysXPhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return { 0, 0, 0 };
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return { 0, 0, 0 };
	return ToFVector(Dyn->getAngularVelocity());
}

void FPhysXPhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	Dyn->setAngularVelocity(ToPxVec3(Vel));
}

// ============================================================
// Mass
// ============================================================

void FPhysXPhysicsScene::SetMass(UPrimitiveComponent* Comp, float Mass)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return;
	PxRigidBodyExt::setMassAndUpdateInertia(*Dyn, Mass);
}

float FPhysXPhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return 1.0f;
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	if (!Dyn) return 1.0f;
	return Dyn->getMass();
}

// ============================================================
// Raycast
// ============================================================

bool FPhysXPhysicsScene::Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit) const
{
	if (!Scene) return false;

	PxRaycastBuffer Hit;
	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist, Hit);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	if (Block.actor && Block.actor->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.actor->userData);
		OutHit.HitActor = OutHit.HitComponent->GetOwner();
	}

	return true;
}
