#include "Physics/PhysXPhysicsScene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Physics/PhysXTypeConversions.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Math/Quat.h"
#include "Object/Object.h"  // IsAliveObject
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>

// PhysX headers
#include <PxPhysicsAPI.h>
#include <pvd/PxPvd.h>
#include <pvd/PxPvdTransport.h>
#include <pvd/PxPvdSceneClient.h>
#include <cooking/PxCooking.h>

using namespace physx;
using namespace PhysXConvert;

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

namespace
{
	FBodyInstance* GetBodyInstanceFromActor(const PxActor* Actor)
	{
		if (!Actor || !Actor->userData)
		{
			return nullptr;
		}

		return static_cast<FBodyInstance*>(Actor->userData);
	}

	AActor* GetOwnerActorFromPhysXActor(const PxActor* Actor)
	{
		FBodyInstance* Body = GetBodyInstanceFromActor(Actor);
		return Body ? Body->GetOwnerActor() : nullptr;
	}

	bool ShouldIgnorePhysXActor(const PxActor* Actor, const AActor* IgnoreActor)
	{
		if (!Actor || !IgnoreActor)
		{
			return false;
		}

		AActor* OwnerActor = GetOwnerActorFromPhysXActor(Actor);
		return OwnerActor == IgnoreActor;
	}

	bool ResolvePhysXRaycastTarget(const PxRaycastHit& Block, FHitResult& OutHit)
	{
		if (Block.shape && Block.shape->userData)
		{
			UPrimitiveComponent* HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
			if (!IsValid(HitComponent))
			{
				return false;
			}

			OutHit.HitComponent = HitComponent;

			AActor* HitActor = HitComponent->GetOwner();
			if (IsValid(HitActor))
			{
				OutHit.HitActor = HitActor;
			}

			return true;
		}

		if (Block.actor)
		{
			AActor* HitActor = GetOwnerActorFromPhysXActor(Block.actor);
			if (!IsValid(HitActor))
			{
				return false;
			}

			OutHit.HitActor = HitActor;
			return true;
		}

		return false;
	}

	bool CreateConvexGeometry(
		PxPhysics* Physics, PxCooking* Cooking, const FBodyShapeDesc& ShapeDesc,
		PxConvexMesh*& OutConvexMesh, PxConvexMeshGeometry& OutGeometry)
	{
		OutConvexMesh = nullptr;
		if (!Physics || !Cooking) return false;
		if (ShapeDesc.ConvexVertices.size() < 4) return false;

		PxConvexMeshDesc ConvexDesc;
		ConvexDesc.points.count = static_cast<PxU32>(ShapeDesc.ConvexVertices.size());
		ConvexDesc.points.stride = sizeof(FVector);
		ConvexDesc.points.data = ShapeDesc.ConvexVertices.data();
		ConvexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
		ConvexDesc.vertexLimit = 255;

		OutConvexMesh = Cooking->createConvexMesh(
			ConvexDesc,
			Physics->getPhysicsInsertionCallback()
		);

		if (!OutConvexMesh) return false;

		const FVector AbsScale = ShapeDesc.ConvexScale.GetAbs();

		const PxMeshScale MeshScale(
			PxVec3(
				std::max(AbsScale.X, 0.001f),
				std::max(AbsScale.Y, 0.001f),
				std::max(AbsScale.Z, 0.001f)
			),
			PxQuat(PxIdentity)
		);

		OutGeometry = PxConvexMeshGeometry(OutConvexMesh, MeshScale);
		if (!OutGeometry.isValid())
		{
			OutConvexMesh->release();
			OutConvexMesh = nullptr;
			return false;
		}

		return true;
	}
}
static PxDefaultAllocator GPhysXAllocator;
static constexpr physx::PxU32 FILTER_FLAG_IGNORE_SAME_OWNER = 1u << 31;

#ifndef WITH_PHYSX_PVD
#define WITH_PHYSX_PVD 1
#endif

static constexpr const char* PHYSX_PVD_HOST = "127.0.0.1";
static constexpr int PHYSX_PVD_PORT = 5425;
static constexpr PxU32 PHYSX_PVD_TIMEOUT_MS = 10;

// ============================================================
// PhysX Foundation/Physics/PVD 싱글턴
// PxCreateFoundation은 프로세스당 1회만 허용 — 복수 Scene에서 공유
// ============================================================
static PxFoundation* GSharedFoundation = nullptr;
static PxPhysics* GSharedPhysics = nullptr;
static bool GSharedExtensionsInitialized = false;

#if WITH_PHYSX_PVD
static PxPvd* GSharedPvd = nullptr;
static PxPvdTransport* GSharedPvdTransport = nullptr;
#endif

static int32 GSharedRefCount = 0;
static bool GSharedVehicleSDKInitialized = false;

static void ReleaseSharedPhysXResources()
{
	if (GSharedVehicleSDKInitialized)
	{
		PxCloseVehicleSDK();
		GSharedVehicleSDKInitialized = false;
	}

	if (GSharedExtensionsInitialized)
	{
		PxCloseExtensions();
		GSharedExtensionsInitialized = false;
	}

	if (GSharedPhysics)
	{
		GSharedPhysics->release();
		GSharedPhysics = nullptr;
	}

#if WITH_PHYSX_PVD
	if (GSharedPvd)
	{
		GSharedPvd->disconnect();
		GSharedPvd->release();
		GSharedPvd = nullptr;
	}

	if (GSharedPvdTransport)
	{
		GSharedPvdTransport->release();
		GSharedPvdTransport = nullptr;
	}
#endif

	if (GSharedFoundation)
	{
		GSharedFoundation->release();
		GSharedFoundation = nullptr;
	}
}

static bool AcquireSharedPhysX(PxFoundation*& OutFoundation, PxPhysics*& OutPhysics)
{
	OutFoundation = nullptr;
	OutPhysics = nullptr;

	if (GSharedRefCount == 0)
	{
		GSharedFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
		if (!GSharedFoundation)
		{
			UE_LOG("[PhysX] Failed to create shared foundation");
			return false;
		}

#if WITH_PHYSX_PVD
		GSharedPvd = PxCreatePvd(*GSharedFoundation);
		if (GSharedPvd)
		{
			GSharedPvdTransport = PxDefaultPvdSocketTransportCreate(
				PHYSX_PVD_HOST,
				PHYSX_PVD_PORT,
				PHYSX_PVD_TIMEOUT_MS
			);

			if (GSharedPvdTransport)
			{
				const bool bConnected = GSharedPvd->connect(
					*GSharedPvdTransport,
					PxPvdInstrumentationFlag::eALL
				);

				UE_LOG(
					"[PhysX] PVD connect %s (%s:%d)",
					bConnected ? "succeeded" : "failed",
					PHYSX_PVD_HOST,
					PHYSX_PVD_PORT
				);
			}
			else
			{
				UE_LOG("[PhysX] Failed to create PVD transport");
			}
		}
		else
		{
			UE_LOG("[PhysX] Failed to create PVD");
		}

		GSharedPhysics = PxCreatePhysics(
			PX_PHYSICS_VERSION,
			*GSharedFoundation,
			PxTolerancesScale(),
			true,
			GSharedPvd
		);
#else
		GSharedPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *GSharedFoundation, PxTolerancesScale());
#endif

		if (!GSharedPhysics)
		{
			UE_LOG("[PhysX] Failed to create shared physics");
			ReleaseSharedPhysXResources();
			return false;
		}

#if WITH_PHYSX_PVD
		GSharedExtensionsInitialized = PxInitExtensions(*GSharedPhysics, GSharedPvd);
#else
		GSharedExtensionsInitialized = PxInitExtensions(*GSharedPhysics, nullptr);
#endif

		if (!GSharedExtensionsInitialized)
		{
			UE_LOG("[PhysX] Failed to initialize PhysX extensions");
			ReleaseSharedPhysXResources();
			return false;
		}

		if (!PxInitVehicleSDK(*GSharedPhysics))
		{
			UE_LOG("[PhysX] Failed to initialize vehicle SDK");
			ReleaseSharedPhysXResources();
			return false;
		}

		GSharedVehicleSDKInitialized = true;
		PxVehicleSetBasisVectors(PxVec3(0.0f, 0.0f, 1.0f), PxVec3(1.0f, 0.0f, 0.0f));
	}

	++GSharedRefCount;
	OutFoundation = GSharedFoundation;
	OutPhysics = GSharedPhysics;
	return true;
}

static void ReleaseSharedPhysX()
{
	if (GSharedRefCount <= 0)
	{
		GSharedRefCount = 0;
		return;
	}

	--GSharedRefCount;
	if (GSharedRefCount == 0)
	{
		ReleaseSharedPhysXResources();
	}
}

// 다른 물체와 충돌하는 필터링 시키기 위한 용도
static void SetupFilterData(PxShape* Shape, const FBodyInstance& Body)
{
	PxFilterData Filter;
	Filter.word0 = static_cast<PxU32>(Body.ObjectType);
	Filter.word1 = 0;
	Filter.word2 = 0;

	if (Body.bIgnoreSameOwner)
	{
		Filter.word2 |= FILTER_FLAG_IGNORE_SAME_OWNER;
	}

	AActor* Owner = nullptr;
	if (Body.OwnerComponent)
	{
		Owner = Body.OwnerComponent->GetOwner();
	}
	else if (Body.OwnerSkeletalComponent)
	{
		Owner = Body.OwnerSkeletalComponent->GetOwner();
	}

	Filter.word3 = IsValid(Owner) ? Owner->GetUUID() : 0;

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		ECollisionResponse R = Body.ResponseContainer.GetResponse(static_cast<ECollisionChannel>(Ch));

		if (R == ECollisionResponse::Block)
		{
			Filter.word1 |= (1u << Ch);
		}
		else if (R == ECollisionResponse::Overlap)
		{
			Filter.word2 |= (1u << Ch);
		}
	}

	Shape->setSimulationFilterData(Filter);
	Shape->setQueryFilterData(Filter);
}

// Trigger용도냐 Shape용도냐 확인하기 위한 용도
static bool ShouldBodyShapeBeTrigger(const FBodyInstance& Body)
{
	if (Body.CollisionEnabled == ECollisionEnabled::NoCollision) return true;
	if (Body.CollisionEnabled == ECollisionEnabled::QueryOnly) return true;

	bool bHasAnyBlockResponse = false;

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		if (Body.ResponseContainer.GetResponse(static_cast<ECollisionChannel>(Ch)) == ECollisionResponse::Block)
		{
			bHasAnyBlockResponse = true;
			break;
		}
	}

	return !bHasAnyBlockResponse;
}


// ============================================================
// PhysX Simulation Event Callback
//
// PhysX 의 onContact / onTrigger 는 Scene->fetchResults(true) 진행 중에 호출되며,
// 그 안에서 직접 게임 측 핸들러(NotifyComponentHit 등)를 호출하면 핸들러가
// World->DestroyActor 같은 scene-mutating 작업을 해서 fetchResults 와 겹쳐 크래쉬한다.
//
// 따라서 콜백은 이벤트를 큐에 적재만 하고, FPhysXPhysicsScene::Tick 의 post-simulate
// 단계 끝에서 DispatchPendingEvents 가 한꺼번에 게임 측 Notify 를 호출한다. 이 시점은
// simulate/fetchResults 외부이므로 핸들러가 자유롭게 actor/component 를 추가/제거해도 안전.
// ============================================================
class FPhysXSimulationCallback : public PxSimulationEventCallback
{
public:
	struct FQueuedHit
	{
		UPrimitiveComponent* Self      = nullptr;  // Notify 가 호출되는 대상
		UPrimitiveComponent* Other     = nullptr;
		FVector              NormalImpulse{0,0,0};
		FHitResult           Hit;
		bool                 bBegin = true;       // false = end
	};

	struct FQueuedTrigger
	{
		UPrimitiveComponent* Self  = nullptr;
		UPrimitiveComponent* Other = nullptr;
		bool                 bBegin = true;        // false = end
	};

	// Block 접촉 → 큐에 적재
	void onContact(const PxContactPairHeader& PairHeader,
		const PxContactPair* Pairs, PxU32 Count) override
	{
		if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0
			|| PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
			return;

		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxContactPair& CP = Pairs[i];
			const bool bBegin = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bEnd = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bEnd) continue;

			auto* CompA = CP.shapes[0] ? static_cast<UPrimitiveComponent*>(CP.shapes[0]->userData) : nullptr;
			auto* CompB = CP.shapes[1] ? static_cast<UPrimitiveComponent*>(CP.shapes[1]->userData) : nullptr;
			if (!CompA || !CompB) continue;

			const ECollisionResponse PairResponse = UPrimitiveComponent::GetMinResponse(CompA, CompB);
			if (PairResponse == ECollisionResponse::Ignore)
			{
				continue;
			}

			// simulation shape끼리 만났지만 채널 응답이 Overlap인 경우가 있다.
			// 이 경우 PhysX는 onContact로 알려줄 수 있으므로 Hit이 아니라 Overlap 큐로 보낸다.
			if (PairResponse == ECollisionResponse::Overlap)
			{
				if (CompA->GetGenerateOverlapEvents())
				{
					PendingTriggers.push_back({ CompA, CompB, bBegin });
				}
				if (CompB->GetGenerateOverlapEvents())
				{
					PendingTriggers.push_back({ CompB, CompA, bBegin });
				}
				continue;
			}

			if (bEnd)
			{
				FQueuedHit A;
				A.Self = CompA;
				A.Other = CompB;
				A.bBegin = false;
				PendingHits.push_back(A);

				FQueuedHit B;
				B.Self = CompB;
				B.Other = CompA;
				B.bBegin = false;
				PendingHits.push_back(B);
				continue;
			}

			PxContactPairPoint ContactPoints[1];
			PxU32 NumPoints = CP.extractContacts(ContactPoints, 1);

			FVector ContactPos(0, 0, 0);
			FVector ContactNormal(0, 0, 1);
			FVector NormalImpulse(0, 0, 0);
			float PenetrationDepth = 0.0f;

			if (NumPoints > 0)
			{
				ContactPos = FVector(ContactPoints[0].position.x, ContactPoints[0].position.y, ContactPoints[0].position.z);
				ContactNormal = FVector(ContactPoints[0].normal.x, ContactPoints[0].normal.y, ContactPoints[0].normal.z);
				NormalImpulse = FVector(ContactPoints[0].impulse.x, ContactPoints[0].impulse.y, ContactPoints[0].impulse.z);
				PenetrationDepth = ContactPoints[0].separation < 0.0f ? -ContactPoints[0].separation : 0.0f;
			}

			FQueuedHit A;
			A.Self = CompA;
			A.Other = CompB;
			A.NormalImpulse = NormalImpulse;
			A.Hit.bHit = true;
			A.Hit.HitComponent = CompB;
			A.Hit.HitActor = CompB->GetOwner();
			A.Hit.WorldHitLocation = ContactPos;
			A.Hit.ImpactNormal = ContactNormal;
			A.Hit.WorldNormal = ContactNormal;
			A.Hit.PenetrationDepth = PenetrationDepth;
			PendingHits.push_back(A);

			FQueuedHit B;
			B.Self = CompB;
			B.Other = CompA;
			B.NormalImpulse = NormalImpulse * -1.0f;
			B.Hit.bHit = true;
			B.Hit.HitComponent = CompA;
			B.Hit.HitActor = CompA->GetOwner();
			B.Hit.WorldHitLocation = ContactPos;
			B.Hit.ImpactNormal = ContactNormal * -1.0f;
			B.Hit.WorldNormal = ContactNormal * -1.0f;
			B.Hit.PenetrationDepth = PenetrationDepth;
			PendingHits.push_back(B);
		}
	}
	// Trigger 진입/이탈 → 큐에 적재
	void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
	{
		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxTriggerPair& TP = Pairs[i];

			if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;

			auto* TriggerComp = TP.triggerShape ? static_cast<UPrimitiveComponent*>(TP.triggerShape->userData) : nullptr;
			auto* OtherComp   = TP.otherShape   ? static_cast<UPrimitiveComponent*>(TP.otherShape->userData)   : nullptr;
			if (!TriggerComp || !OtherComp) continue;

			const bool bBegin = (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bEnd   = (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bEnd) continue;

			if (TriggerComp->GetGenerateOverlapEvents())
			{
				PendingTriggers.push_back({ TriggerComp, OtherComp, bBegin });
			}
			if (OtherComp->GetGenerateOverlapEvents())
			{
				PendingTriggers.push_back({ OtherComp, TriggerComp, bBegin });
			}
		}
	}

	// FPhysXPhysicsScene::Tick 끝에서 호출. simulate/fetchResults 바깥이므로 핸들러가
	// 자유롭게 World->DestroyActor / SpawnActor / RegisterComponent 호출 가능.
	// 핸들러 도중 다른 컴포넌트가 destroy되는 경우 대비해 dispatch 직전에 IsAliveObject
	// 검증 — destroy된 포인터를 만지지 않는다.
	void DispatchPendingEvents()
	{
		// move-out — dispatch 도중 새 이벤트가 큐에 들어오는 일은 없지만, 안전하게 swap 후 처리.
		std::vector<FQueuedHit> HitsToDispatch;
		HitsToDispatch.swap(PendingHits);
		std::vector<FQueuedTrigger> TriggersToDispatch;
		TriggersToDispatch.swap(PendingTriggers);

		for (FQueuedHit& E : HitsToDispatch)
		{
			if (!IsValid(E.Self) || !IsValid(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (!IsValid(OtherActor)) continue;
			if (E.bBegin)
			{
				E.Self->NotifyComponentHit(E.Self, OtherActor, E.Other, E.NormalImpulse, E.Hit);
			}
			else
			{
				E.Self->NotifyComponentEndHit(E.Self, OtherActor, E.Other);
			}
		}

		for (FQueuedTrigger& E : TriggersToDispatch)
		{
			if (!IsValid(E.Self) || !IsValid(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (!IsValid(OtherActor)) continue;
			if (E.bBegin)
			{
				FHitResult DummyHit;
				E.Self->NotifyComponentBeginOverlap(E.Self, OtherActor, E.Other, 0, false, DummyHit);
			}
			else
			{
				E.Self->NotifyComponentEndOverlap(E.Self, OtherActor, E.Other, 0);
			}
		}
	}

	void ClearPendingEvents()
	{
		PendingHits.clear();
		PendingTriggers.clear();
	}

	void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
	void onWake(PxActor**, PxU32) override {}
	void onSleep(PxActor**, PxU32) override {}
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

private:
	std::vector<FQueuedHit>     PendingHits;
	std::vector<FQueuedTrigger> PendingTriggers;
};

// ============================================================
// Transform 변환 유틸
// ============================================================
static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
	FTransform WorldTransform = FTransform::FromMatrixWithScale(Comp->GetWorldMatrix());
	WorldTransform.Scale = FVector::OneVector;
	return ToPxTransform(WorldTransform);
}

static void SetComponentWorldPose(UPrimitiveComponent* Comp, const PxTransform& Pose)
{
	if (!IsValid(Comp))
	{
		return;
	}

	const FVector NewPos = ToFVector(Pose.p);
	const FQuat NewWorldQuat = ToFQuat(Pose.q);

	Comp->SetWorldLocation(NewPos);

	if (USceneComponent* Parent = Comp->GetParent())
	{
		const FQuat ParentWorldQuat = FQuat::FromMatrix(Parent->GetWorldMatrix());

		// 이 프로젝트의 기존 이동 코드에서 쓰는 row-vector 계열 합성 방식에 맞춤.
		Comp->SetRelativeRotation((NewWorldQuat * ParentWorldQuat.Inverse()).GetNormalized());
	}
	else
	{
		Comp->SetRelativeRotation(NewWorldQuat);
	}
}

// PxFilterShader — 엔진의 채널/응답 매트릭스를 PhysX에서 처리
// 양쪽 모두 상대 채널에 대해 Block이면 물리 충돌, 한쪽이라도 Overlap이면 트리거, 그 외 무시
static PxFilterFlags KraftonFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* /*constantBlock*/, PxU32 /*constantBlockSize*/)
{
	const bool bSameOwner =
		filterData0.word3 != 0 &&
		filterData0.word3 == filterData1.word3;

	const bool bIgnoreSameOwner =
		((filterData0.word2 & FILTER_FLAG_IGNORE_SAME_OWNER) != 0) ||
		((filterData1.word2 & FILTER_FLAG_IGNORE_SAME_OWNER) != 0);

	if (bSameOwner && bIgnoreSameOwner)
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
			| PxPairFlag::eNOTIFY_TOUCH_LOST
			| PxPairFlag::eNOTIFY_CONTACT_POINTS;
		return PxFilterFlag::eDEFAULT;
	}

	// 한쪽이라도 Overlap → 겹침 감지만 (물리적 밀어내기 없음).
	// 일반적으로 이 케이스는 위 trigger shape 분기에서 이미 처리되지만, 등록 시점에
	// trigger flag로 분류되지 않은 simulation shape pair인데 응답이 Overlap인 경우의
	// 안전망. eSOLVE_CONTACT 명시 제외 + eDETECT_DISCRETE_CONTACT + NOTIFY로 detection만.
	bool bAOverlapsB = (filterData0.word2 & (1u << channelB)) != 0;
	bool bBOverlapsA = (filterData1.word2 & (1u << channelA)) != 0;

	if (bAOverlapsB || bBOverlapsA)
	{
		pairFlags = PxPairFlag::eDETECT_DISCRETE_CONTACT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_LOST;
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
	// 재초기화 경로가 들어와도 shared PhysX ref-count가 깨지지 않도록 먼저 정리한다.
	Shutdown();

	World = InWorld;
	bShutdownComplete = false;

	// Foundation / Physics — 프로세스 싱글턴 공유
	if (!AcquireSharedPhysX(Foundation, Physics))
	{
		UE_LOG("[PhysX] Failed to create Foundation or Physics");
		bShutdownComplete = true;
		World = nullptr;
		return;
	}
	bSharedPhysXAcquired = true;

	// Convex cooking
	PxCookingParams CookingParams(Physics->getTolerancesScale());
	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, CookingParams);
	if (!Cooking)
	{
		UE_LOG("[PhysX] Failed to create Cooking");
		Shutdown();
		return;
	}

	// CPU Dispatcher
	Dispatcher = PxDefaultCpuDispatcherCreate(4);
	if (!Dispatcher)
	{
		UE_LOG("[PhysX] Failed to create CPU dispatcher");
		Shutdown();
		return;
	}

	// Event callback
	EventCallback = new FPhysXSimulationCallback();

	// Scene
	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Z-up, m 단위
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = KraftonFilterShader;
	SceneDesc.simulationEventCallback = EventCallback;

	// Active Actor 사용
	SceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	// 안정화 옵션
	SceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
	SceneDesc.flags |= PxSceneFlag::eENABLE_PCM;

	Scene = Physics->createScene(SceneDesc);

	if (!Scene)
	{
		UE_LOG("[PhysX] Failed to create Scene");
		Shutdown();
		return;
	}

#if WITH_PHYSX_PVD
	if (PxPvdSceneClient* PvdClient = Scene->getScenePvdClient())
	{
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);

		UE_LOG("[PhysX] PVD scene flags enabled");
	}
	else
	{
		UE_LOG("[PhysX] PVD scene client is null");
	}
#endif

	// Default material (static friction, dynamic friction, restitution)
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.3f);
	if (!DefaultMaterial)
	{
		UE_LOG("[PhysX] Failed to create default material");
		Shutdown();
		return;
	}

	UE_LOG("[PhysX] Initialized successfully (Scene=%p)", Scene);
}

void FPhysXPhysicsScene::Shutdown()
{
	if (bShutdownComplete) return;
	bShutdownComplete = true;

	if (EventCallback) EventCallback->ClearPendingEvents();
	if (Scene) Scene->setSimulationEventCallback(nullptr);

	ReleaseRegisteredBodies();

	if (Scene)
	{
		Scene->release();
		Scene = nullptr;
	}

	if (DefaultMaterial)
	{
		DefaultMaterial->release();
		DefaultMaterial = nullptr;
	}

	if (Cooking)
	{
		Cooking->release();
		Cooking = nullptr;
	}

	if (EventCallback)
	{
		delete EventCallback;
		EventCallback = nullptr;
	}

	if (Dispatcher)
	{
		Dispatcher->release();
		Dispatcher = nullptr;
	}

	Foundation = nullptr;
	Physics = nullptr;
	World = nullptr;

	if (bSharedPhysXAcquired)
	{
		bSharedPhysXAcquired = false;
		ReleaseSharedPhysX();
	}
}

void FPhysXPhysicsScene::RegisterComponent(UPrimitiveComponent* Comp)
{
	if (!IsValid(Comp) || !Scene || !Physics || !DefaultMaterial) return;

	// NoCollision이면 body 만들 필요 없음.
	if (Comp->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;
	

	FBodyInstance& Body = Comp->GetBodyInstance();
	if (Body.IsValidBodyInstance()) return;

	Body.OwnerComponent = Comp;
	Body.OwnerSkeletalComponent = nullptr;
	Body.BoneName = FName::None;
	Body.BoneIndex = -1;

	FBodyInstanceInitDesc Desc;
	if (!BuildBodyInstanceInitDescFromPrimitive(Comp, Desc)) return;

	if (!CreateBodyInstance(Body, Desc)) return;

	AddRegisteredBody(&Body);
}

void FPhysXPhysicsScene::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!IsValid(Comp)) return;

	FBodyInstance& Body = Comp->GetBodyInstance();

	RemoveRegisteredBody(&Body);
	DestroyBodyInstance(Body);
}

void FPhysXPhysicsScene::RebuildBody(UPrimitiveComponent* Comp)
{
	if (!IsValid(Comp)) return;

	UnregisterComponent(Comp);
	RegisterComponent(Comp);
}

bool FPhysXPhysicsScene::CreateBodyInstance(FBodyInstance& Body, const FBodyInstanceInitDesc& Desc)
{
	if (!Scene || !Physics || !DefaultMaterial)
	{
		return false;
	}

	DestroyBodyInstance(Body);

	// Desc 값 Body에 복사
	Body.bSimulatePhysics = Desc.bSimulatePhysics;
	Body.bKinematic = Desc.bKinematic;
	Body.CollisionEnabled = Desc.CollisionEnabled;
	Body.ObjectType = Desc.ObjectType;
	Body.ResponseContainer = Desc.ResponseContainer;
	Body.bIgnoreSameOwner = Desc.bIgnoreSameOwner;
	Body.Mass = Desc.Mass;
	Body.CenterOfMassOffset = Desc.CenterOfMassOffset;
	Body.LinearDamping = Desc.LinearDamping;
	Body.AngularDamping = Desc.AngularDamping;
	Body.bEnableGravity = Desc.bEnableGravity;
	Body.InertiaTensorScale = Desc.InertiaTensorScale;

	const PxTransform ActorPose = ToPxTransform(Desc.WorldTransform);

	PxRigidActor* Actor = nullptr;

	if (Desc.bSimulatePhysics)
	{
		PxRigidDynamic* Dynamic = Physics->createRigidDynamic(ActorPose);
		if (!Dynamic)
		{
			return false;
		}

		Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, Desc.bKinematic);
		Actor = Dynamic;
	}
	else
	{
		Actor = Physics->createRigidStatic(ActorPose);
		if (!Actor)
		{
			return false;
		}
	}

	Actor->userData = &Body;

	// Shape들 Body에 저장
	for (const FBodyShapeDesc& ShapeDesc : Desc.Shapes)
	{
		PxGeometryHolder Geom;
		bool bHasGeom = false;
		PxQuat ShapeAxisRot = PxQuat(PxIdentity);

		PxConvexMesh* TempConvexMesh = nullptr;

		switch (ShapeDesc.ShapeType)
		{
		case EBodyInstanceShapeType::Sphere:
			Geom.storeAny(PxSphereGeometry(std::max(ShapeDesc.SphereRadius, 0.001f)));
			bHasGeom = true;
			break;

		case EBodyInstanceShapeType::Box:
			Geom.storeAny(PxBoxGeometry(
				std::max(ShapeDesc.BoxHalfExtent.X, 0.001f),
				std::max(ShapeDesc.BoxHalfExtent.Y, 0.001f),
				std::max(ShapeDesc.BoxHalfExtent.Z, 0.001f)
			));
			bHasGeom = true;
			break;

		case EBodyInstanceShapeType::Capsule:
		{
			const float Radius = std::max(ShapeDesc.CapsuleRadius, 0.001f);
			const float HalfHeightWithoutCaps = std::max(ShapeDesc.CapsuleHalfHeight - Radius, 0.001f);

			Geom.storeAny(PxCapsuleGeometry(Radius, HalfHeightWithoutCaps));

			// 엔진 Capsule은 Z축 기준, PhysX Capsule은 X축 기준
			ShapeAxisRot = PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));

			bHasGeom = true;
			break;
		}

		case EBodyInstanceShapeType::Convex:
		{
			PxConvexMeshGeometry ConvexGeometry;
			if (!CreateConvexGeometry(Physics, Cooking, ShapeDesc, TempConvexMesh, ConvexGeometry))
			{
				break;
			}

			Geom.storeAny(ConvexGeometry);
			bHasGeom = true;
			break;
		}

		default:
			break;
		}

		if (!bHasGeom)
		{
			if (TempConvexMesh)
			{
				TempConvexMesh->release();
				TempConvexMesh = nullptr;
			}
			continue;
		}

		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Actor, Geom.any(), *DefaultMaterial);
		if (TempConvexMesh)
		{
			TempConvexMesh->release();
			TempConvexMesh = nullptr;
		}

		if (!Shape)
		{
			continue;
		}


		PxTransform LocalPose = ToPxTransform(ShapeDesc.LocalTransform);
		LocalPose.q = LocalPose.q * ShapeAxisRot;
		Shape->setLocalPose(LocalPose);

		SetupFilterData(Shape, Body);

		const bool bQueryEnabled = Body.CollisionEnabled == ECollisionEnabled::QueryOnly
			|| Body.CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
		const bool bPhysicsEnabled = Body.CollisionEnabled == ECollisionEnabled::PhysicsOnly
			|| Body.CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
		const bool bUseTrigger = ShouldBodyShapeBeTrigger(Body);

		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bQueryEnabled);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bPhysicsEnabled && !bUseTrigger);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, bUseTrigger);

		Shape->userData = Body.OwnerComponent
			? static_cast<UPrimitiveComponent*>(Body.OwnerComponent)
			: static_cast<UPrimitiveComponent*>(Body.OwnerSkeletalComponent);

		Body.Shapes.push_back(Shape);
	}

	if (Body.Shapes.empty())
	{
		Actor->release();
		return false;
	}

	if (PxRigidDynamic* Dynamic = Actor->is<PxRigidDynamic>())
	{
		const float MassKg = Desc.Mass > 0.0f ? Desc.Mass : 1.0f;
		PxVec3 LocalCOM = ToPxVec3(Desc.CenterOfMassOffset);

		PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, MassKg, &LocalCOM);
		Dynamic->setCMassLocalPose(PxTransform(LocalCOM));
		Dynamic->setLinearDamping(std::max(Desc.LinearDamping, 0.0f));
		Dynamic->setAngularDamping(std::max(Desc.AngularDamping, 0.0f));
		Dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !Desc.bEnableGravity);

		PxVec3 Inertia = Dynamic->getMassSpaceInertiaTensor();
		Inertia.x *= std::max(Desc.InertiaTensorScale.X, 0.001f);
		Inertia.y *= std::max(Desc.InertiaTensorScale.Y, 0.001f);
		Inertia.z *= std::max(Desc.InertiaTensorScale.Z, 0.001f);
		Dynamic->setMassSpaceInertiaTensor(Inertia);
	}

	Scene->addActor(*Actor);

	Body.RigidActor = Actor;
	return true;
}

void FPhysXPhysicsScene::DestroyBodyInstance(FBodyInstance& Body)
{
	RemoveRegisteredBody(&Body);
	PxRigidActor* Actor = Body.RigidActor;

	if (!Actor)
	{
		Body.ClearPhysicsPointers();
		return;
	}

	Actor->userData = nullptr;

	const PxU32 NumShapes = Actor->getNbShapes();
	if (NumShapes > 0)
	{
		std::vector<PxShape*> Shapes(NumShapes);
		Actor->getShapes(Shapes.data(), NumShapes);

		for (PxShape* Shape : Shapes)
		{
			if (Shape)
			{
				Shape->userData = nullptr;
			}
		}
	}

	if (Scene)
	{
		Scene->removeActor(*Actor);
	}

	Actor->release();
	Body.ClearPhysicsPointers();
}

bool FPhysXPhysicsScene::CreateConstraintInstance(FConstraintInstance& Constraint)
{
	if (!Physics || !Constraint.ParentBody || !Constraint.ChildBody)
	{
		return false;
	}

	DestroyConstraintInstance(Constraint);

	PxRigidActor* ParentActor = Constraint.ParentBody->RigidActor;
	PxRigidActor* ChildActor = Constraint.ChildBody->RigidActor;

	if (!ParentActor || !ChildActor)
	{
		return false;
	}

	PxTransform ParentFrame = ToPxTransform(Constraint.ParentFrame);
	PxTransform ChildFrame = ToPxTransform(Constraint.ChildFrame);

	PxD6Joint* Joint = PxD6JointCreate(
		*Physics,
		ParentActor,
		ParentFrame,
		ChildActor,
		ChildFrame
	);

	if (!Joint)
	{
		return false;
	}

	Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
	Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
	Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

	Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
	Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
	Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

	auto ClampDegrees = [](float Degrees, float MinDegrees, float MaxDegrees)
	{
		return std::max(MinDegrees, std::min(Degrees, MaxDegrees));
	};


	const float Twist = ClampDegrees(Constraint.TwistLimitDegrees, 0.0f, 170.0f) * DEG_TO_RAD;
	const float Swing1 = ClampDegrees(Constraint.Swing1LimitDegrees, 0.0f, 170.0f) * DEG_TO_RAD;
	const float Swing2 = ClampDegrees(Constraint.Swing2LimitDegrees, 0.0f, 170.0f) * DEG_TO_RAD;

	// 비틀기 제한
	Joint->setTwistLimit(PxJointAngularLimitPair(-Twist, Twist));
	// 꺾이는 각도 제한(Cone 형태 모양 제한)
	Joint->setSwingLimit(PxJointLimitCone(Swing1, Swing2));

	// 보통 false로 설정 -> 인접한 두 Bone끼리 충돌 시킬것인지에 대한 처리.
	// 손과 머리같은 연결되지 않은 body끼리의 충돌은 Collision Filtering으로 처리하는것이 더 좋다함.
	Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, Constraint.bEnableCollision);

	Joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);

	if (Constraint.bEnableProjection)
	{
		// joint가 위치상으로 얼마나 벌어졌을 때 강제 보정할지 정하는 값
		Joint->setProjectionLinearTolerance(
			std::max(Constraint.ProjectionLinearTolerance, 0.0f)
		);

		// joint가 회전상으로 얼마나 틀어졌을 때 강제 보정할지 정하는 값(회전 한계 이상으로 갔을때 되돌려 놓는 값)
		Joint->setProjectionAngularTolerance(
			std::max(Constraint.ProjectionAngularToleranceDegrees, 0.0f) * DEG_TO_RAD
		);

		Joint->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);
	}
	else
	{
		Joint->setConstraintFlag(PxConstraintFlag::ePROJECTION, false);
	}

	Constraint.Joint = Joint;
	return true;
}

void FPhysXPhysicsScene::DestroyConstraintInstance(FConstraintInstance& Constraint)
{
	if (Constraint.Joint)
	{
		Constraint.Joint->release();
		Constraint.ClearPhysicsPointers();
	}
}

void FPhysXPhysicsScene::Tick(float DeltaTime)
{
	if (bShutdownComplete || !Scene || DeltaTime <= 0.0f)
	{
		return;
	}

	constexpr float MaxPhysicsDeltaTime = 0.1f;
	DeltaTime = std::min(DeltaTime, MaxPhysicsDeltaTime);

	SyncEngineToPhysicsBeforeSim();
	SimulatePhysics(DeltaTime);
	SyncPhysicsToEngineAfterSim();
	DispatchPhysicsEvents();
}

bool FPhysXPhysicsScene::Sweep(const FVector& Start, const FVector& Dir, float MaxDist, const FCollisionShape& Shape, const FQuat& ShapeRot, FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (!Scene) return false;

	// Raycast의 FChannelRaycastFilter와 동일한 로직, Sweep용으로 재선언
	struct FChannelSweepFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 TraceBit = 0;
		FChannelSweepFilter(const AActor* InIgnoreActor, ECollisionChannel InChannel)
			: IgnoreActor(InIgnoreActor)
			, TraceBit(1u << static_cast<PxU32>(InChannel))
		{
		}
		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (ShouldIgnorePhysXActor(Actor, IgnoreActor))
			{
				return PxQueryHitType::eNONE;
			}

			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				if ((ShapeData.word1 & TraceBit) == 0)
					return PxQueryHitType::eNONE;
			}
			return PxQueryHitType::eBLOCK;
		}
		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	// FCollisionShape → PxGeometry 변환
	// GeometryHolder로 스택에 geometry 보관 (Sphere / Capsule / Box 지원)
	PxGeometryHolder GeomHolder;
	PxQuat PxShapeQuat = ToPxQuat(ShapeRot);

	switch (Shape.ShapeType)
	{
	case ECollisionShape::Sphere:
	{
		GeomHolder.storeAny(PxSphereGeometry(
			std::max(Shape.GetSphereRadius(), 0.001f)
		));
		break;
	}

	case ECollisionShape::Capsule:
	{
		const float Radius = std::max(Shape.GetCapsuleRadius(), 0.001f);
		const float CapsuleHalfHeight = std::max(Shape.GetCapsuleHalfHeight(), Radius + 0.001f);

		// Engine capsule half height: 구 포함.
		// PhysX capsule half height: 구 제외 실린더 half length.
		const float HalfHeightWithoutCaps =
			std::max(CapsuleHalfHeight - Radius, 0.001f);

		GeomHolder.storeAny(PxCapsuleGeometry(Radius, HalfHeightWithoutCaps));

		// Sweep query도 body 생성과 같은 축 보정을 적용해야 함.
		PxShapeQuat = PxShapeQuat * PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
		break;
	}

	case ECollisionShape::Box:
	{
		const FVector Extent = Shape.GetExtent();
		GeomHolder.storeAny(PxBoxGeometry(
			std::max(Extent.X, 0.001f),
			std::max(Extent.Y, 0.001f),
			std::max(Extent.Z, 0.001f)
		));
		break;
	}

	default:
		return false;
	}

	const PxTransform PxStartPose(ToPxVec3(Start), PxShapeQuat);
	const PxVec3 PxDir = ToPxVec3(Dir);

	PxSweepBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FChannelSweepFilter FilterCallback(IgnoreActor, TraceChannel);

	bool bStatus = Scene->sweep(
		GeomHolder.any(),    // sweep geometry
		PxStartPose,         // 시작 pose (위치 + 회전)
		PxDir,               // 방향 (unit vector)
		MaxDist,             // 최대 거리
		Hit,
		PxHitFlag::eDEFAULT,
		FilterData,
		&FilterCallback
	);

	if (!bStatus || !Hit.hasBlock) return false;

	const PxSweepHit& Block = Hit.block;
	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(PxStartPose.p) + ToFVector(PxDir) * Block.distance;
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	// distance == 0 이면 시작 지점에서 이미 겹침 (initial overlap)
	OutHit.bStartPenetrating = Block.distance <= 0.0f;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor)
	{
		OutHit.HitActor = GetOwnerActorFromPhysXActor(Block.actor);
	}

	return true;
}

// ============================================================
// Force / Torque
// ============================================================

void FPhysXPhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().AddForce(Force);
}

void FPhysXPhysicsScene::AddImpulse(UPrimitiveComponent* Comp, const FVector& Impulse)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().AddImpulse(Impulse);
}

void FPhysXPhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().SetLinearVelocity(Vel);
}

FVector FPhysXPhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
	if (!IsValid(Comp)) return FVector::ZeroVector;
	return Comp->GetBodyInstance().GetLinearVelocity();
}

void FPhysXPhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().SetAngularVelocity(Vel);
}

FVector FPhysXPhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
	if (!IsValid(Comp)) return FVector::ZeroVector;
	return Comp->GetBodyInstance().GetAngularVelocity();
}

void FPhysXPhysicsScene::SetMass(UPrimitiveComponent* Comp, float NewMass)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().SetMass(NewMass);
}

float FPhysXPhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
	if (!IsValid(Comp)) return 0.0f;
	return Comp->GetBodyInstance().GetMass();
}

void FPhysXPhysicsScene::SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().SetCenterOfMass(LocalOffset);
}

FVector FPhysXPhysicsScene::GetCenterOfMass(UPrimitiveComponent* Comp) const
{
	if (!IsValid(Comp)) return FVector::ZeroVector;
	return Comp->GetBodyInstance().GetCenterOfMass();
}

void FPhysXPhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().AddForceAtLocation(Force, WorldLocation);
}

void FPhysXPhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
	if (!IsValid(Comp)) return;
	Comp->GetBodyInstance().AddTorque(Torque);
}

// ============================================================
// Raycast
// ============================================================

bool FPhysXPhysicsScene::Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (!Scene) return false;

	// Channel + IgnoreActor 통합 filter.
	// shape의 queryFilterData는 SetupFilterData에서 word0=ObjectType, word1=Block 마스크.
	// 응답이 TraceChannel에 대해 Block(=word1의 해당 비트 set)인 shape만 hit으로 인정.
	// trigger flag가 set된 shape는 PhysX 측 query에서 자동 제외되므로 별도 처리 불필요.
	struct FChannelRaycastFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 TraceBit = 0;

		FChannelRaycastFilter(const AActor* InIgnoreActor, ECollisionChannel InChannel)
			: IgnoreActor(InIgnoreActor)
			, TraceBit(1u << static_cast<PxU32>(InChannel))
		{
		}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (ShouldIgnorePhysXActor(Actor, IgnoreActor))
			{
				return PxQueryHitType::eNONE;
			}

			// shape의 응답이 TraceChannel에 대해 Block인지 확인.
			// (word1[TraceChannel 비트]가 set이면 Block 응답)
			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				if ((ShapeData.word1 & TraceBit) == 0)
				{
					return PxQueryHitType::eNONE;
				}
			}

			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	PxRaycastBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FChannelRaycastFilter FilterCallback(IgnoreActor, TraceChannel);

	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist, Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	if (!ResolvePhysXRaycastTarget(Block, OutHit))
	{
		return false;
	}

	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	return true;
}

bool FPhysXPhysicsScene::RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	uint32 ObjectTypeMask, const AActor* IgnoreActor) const
{
	if (!Scene || ObjectTypeMask == 0) return false;

	// SetupFilterData (line ~322) 에서 word0 = ObjectType (채널 enum 값) 으로 set.
	// ObjectType 마스크 비트 검사로 hit 후보 필터.
	// Trigger flag shape 는 PhysX 측 query 단계에서 자동 제외.
	struct FObjectTypeRaycastFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 ObjectTypeMask = 0;

		FObjectTypeRaycastFilter(const AActor* InIgnoreActor, PxU32 InMask)
			: IgnoreActor(InIgnoreActor)
			, ObjectTypeMask(InMask)
		{
		}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (ShouldIgnorePhysXActor(Actor, IgnoreActor))
			{
				return PxQueryHitType::eNONE;
			}
			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				const PxU32 ShapeObjectBit = 1u << ShapeData.word0;
				if ((ShapeObjectBit & ObjectTypeMask) == 0)
				{
					return PxQueryHitType::eNONE;
				}
			}
			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	PxRaycastBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FObjectTypeRaycastFilter FilterCallback(IgnoreActor, ObjectTypeMask);

	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist, Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	if (!ResolvePhysXRaycastTarget(Block, OutHit))
	{
		return false;
	}

	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	return true;
}

void FPhysXPhysicsScene::AddRegisteredBody(FBodyInstance* Body)
{
	if (!Body) return;

	auto It = std::find(RegisteredBodies.begin(), RegisteredBodies.end(), Body);
	if (It != RegisteredBodies.end()) return;

	RegisteredBodies.push_back(Body);
}

void FPhysXPhysicsScene::RemoveRegisteredBody(FBodyInstance* Body)
{
	if (!Body) return;

	RegisteredBodies.erase(
		std::remove(RegisteredBodies.begin(), RegisteredBodies.end(), Body),
		RegisteredBodies.end()
	);
}

void FPhysXPhysicsScene::ReleaseRegisteredBodies()
{
	std::vector<FBodyInstance*> BodiesToDestroy;
	BodiesToDestroy.swap(RegisteredBodies);

	for (FBodyInstance* Body : BodiesToDestroy)
	{
		if (!Body) continue;
		DestroyBodyInstance(*Body);
	}
}

FBodyInstance* FPhysXPhysicsScene::FindRegisteredBodyByActor(const PxActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	for (FBodyInstance* Body : RegisteredBodies)
	{
		if (!Body || !Body->IsValidBodyInstance())
		{
			continue;
		}

		if (Body->RigidActor == Actor)
		{
			return Body;
		}
	}

	return nullptr;
}

void FPhysXPhysicsScene::SyncEngineToPhysicsBeforeSim()
{
	constexpr float TeleportPosThresholdSq = 0.001f;
	constexpr float TeleportRotThreshold = 0.99f;

	for (FBodyInstance* Body : RegisteredBodies)
	{
		if (!Body || !Body->IsValidBodyInstance()) continue;

		UPrimitiveComponent* Comp = Body->OwnerComponent;
		if (!IsValid(Comp)) continue;

		PxRigidActor* Actor = Body->RigidActor;
		if (!Actor) continue;

		const PxTransform NewPose = GetPxTransform(Comp);

		if (PxRigidDynamic* Dynamic = Actor->is<PxRigidDynamic>())
		{
			if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
			{
				Dynamic->setKinematicTarget(NewPose);
			}
			else
			{
				const PxTransform PxPose = Dynamic->getGlobalPose();

				const PxVec3 DeltaP = NewPose.p - PxPose.p;
				const float DistSq =
					DeltaP.x * DeltaP.x +
					DeltaP.y * DeltaP.y +
					DeltaP.z * DeltaP.z;

				const float QDot = std::abs(
					NewPose.q.x * PxPose.q.x +
					NewPose.q.y * PxPose.q.y +
					NewPose.q.z * PxPose.q.z +
					NewPose.q.w * PxPose.q.w
				);

				if (DistSq > TeleportPosThresholdSq || QDot < TeleportRotThreshold)
				{
					Dynamic->setGlobalPose(NewPose);
				}
			}
		}
		else if (Actor->is<PxRigidStatic>())
		{
			Actor->setGlobalPose(NewPose);
		}
	}
}

void FPhysXPhysicsScene::SimulatePhysics(float DeltaTime)
{
	Scene->simulate(DeltaTime);
	Scene->fetchResults(true);
}

void FPhysXPhysicsScene::SyncPhysicsToEngineAfterSim()
{
	PxU32 NumActiveActors = 0;
	PxActor** ActiveActors = Scene->getActiveActors(NumActiveActors);

	for (PxU32 i = 0; i < NumActiveActors; ++i)
	{
		PxActor* ActiveActor = ActiveActors[i];
		if (!ActiveActor)
		{
			continue;
		}

		PxRigidDynamic* Dynamic = ActiveActor->is<PxRigidDynamic>();
		if (!Dynamic)
		{
			continue;
		}

		if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
		{
			continue;
		}

		FBodyInstance* Body = FindRegisteredBodyByActor(ActiveActor);
		if (!Body || !Body->IsValidBodyInstance())
		{
			continue;
		}

		// 일반 PrimitiveComponent만 여기서 Component transform sync.
		// SkeletalMesh ragdoll body는 OwnerSkeletalComponent 쪽이므로 여기서 제외됨.
		UPrimitiveComponent* Comp = Body->OwnerComponent;
		if (!IsValid(Comp))
		{
			continue;
		}

		const PxTransform Pose = Dynamic->getGlobalPose();
		SetComponentWorldPose(Comp, Pose);
	}
}

void FPhysXPhysicsScene::DispatchPhysicsEvents()
{
	if (EventCallback)
	{
		EventCallback->DispatchPendingEvents();
	}
}
