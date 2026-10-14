#include "Physics/PhysicsQueryFilter.h"
#include "Physics/PhysicsFilterData.h"

#include "Component/PrimitiveComponent.h"

namespace
{
	bool IsQueryEnabled(const physx::PxFilterData& Data)
	{
		return (Data.word3 & EPhysicsFilterFlags::QueryOnly) ||
			(Data.word3 & EPhysicsFilterFlags::QueryAndPhysics);
	}
}

UPrimitiveComponent* GetComponentFromQueryShape(const physx::PxShape* Shape)
{
	return Shape ? static_cast<UPrimitiveComponent*>(Shape->userData) : nullptr;
}

FPhysicsRaycastFilterCallback::FPhysicsRaycastFilterCallback(ECollisionChannel TraceChannel, const AActor* IgnoreActor)
	: TraceChannel(TraceChannel), IgnoreActor(IgnoreActor)
{
}

physx::PxQueryHitType::Enum FPhysicsRaycastFilterCallback::preFilter(const physx::PxFilterData& FilterData, const physx::PxShape* Shape,
	const physx::PxRigidActor* Actor, physx::PxHitFlags& QueryFlags)
{
	UPrimitiveComponent* Component = GetComponentFromQueryShape(Shape);
	if (!Component) return physx::PxQueryHitType::eNONE;

	if (IgnoreActor && Component->GetOwner() == IgnoreActor) return physx::PxQueryHitType::eNONE;

	const physx::PxFilterData ShapeFilterData = Shape->getQueryFilterData();

	if (!IsQueryEnabled(ShapeFilterData)) return physx::PxQueryHitType::eNONE;

	if (Component->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) return physx::PxQueryHitType::eNONE;

	return physx::PxQueryHitType::eBLOCK;
}

physx::PxQueryHitType::Enum FPhysicsRaycastFilterCallback::postFilter(const physx::PxFilterData& FilterData, const physx::PxQueryHit& Hit)
{
	return physx::PxQueryHitType::eBLOCK;
}

FPhysicsOverlapFilterCallback::FPhysicsOverlapFilterCallback(ECollisionChannel InTraceChannel, const AActor* InIgnoreActor)
	: TraceChannel(InTraceChannel), IgnoreActor(InIgnoreActor)
{
}

physx::PxQueryHitType::Enum FPhysicsOverlapFilterCallback::preFilter(const physx::PxFilterData& FilterData, const physx::PxShape* Shape,
	const physx::PxRigidActor* Actor, physx::PxHitFlags& QueryFlags)
{
	UPrimitiveComponent* Component = GetComponentFromQueryShape(Shape);
	if (!Component) return physx::PxQueryHitType::eNONE;

	if (IgnoreActor && Component->GetOwner() == IgnoreActor) return physx::PxQueryHitType::eNONE;

	const physx::PxFilterData ShapeFilterData = Shape->getQueryFilterData();

	if (!IsQueryEnabled(ShapeFilterData)) return physx::PxQueryHitType::eNONE;

	const ECollisionResponse Response = Component->GetCollisionResponseToChannel(TraceChannel);
	if (Response == ECollisionResponse::Ignore) return physx::PxQueryHitType::eNONE;

	return physx::PxQueryHitType::eTOUCH;
}

physx::PxQueryHitType::Enum FPhysicsOverlapFilterCallback::postFilter(const physx::PxFilterData& FilterData, const physx::PxQueryHit& Hit)
{
	return physx::PxQueryHitType::eTOUCH;
}
