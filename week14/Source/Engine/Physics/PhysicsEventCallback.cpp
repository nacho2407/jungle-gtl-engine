#include "Physics/PhysicsEventCallback.h"
#include "Physics/PhysXConversions.h"

#include "Component/PrimitiveComponent.h"

namespace
{

	UPrimitiveComponent* GetComponentFromActor(const physx::PxRigidActor* Actor)
	{
		if (!Actor || !Actor->userData) return nullptr;

		FBodyInstance* BodyInstance = static_cast<FBodyInstance*>(Actor->userData);
		return BodyInstance ? BodyInstance->OwnerComponent : nullptr;
	}

	UPrimitiveComponent* GetComponentFromShape(const physx::PxShape* Shape)
	{
		return Shape ? static_cast<UPrimitiveComponent*>(Shape->userData) : nullptr;
	}

	bool IsRemovedTriggerPair(const physx::PxTriggerPair& Pair)
	{
		return (Pair.flags & physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER) ||
			(Pair.flags & physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER);
	}

}

void FPhysicsEventCallback::onContact(const physx::PxContactPairHeader& PairHeader,
	const physx::PxContactPair* Pairs, physx::PxU32 Count)
{
	UPrimitiveComponent* CompA = GetComponentFromActor(PairHeader.actors[0]);
	UPrimitiveComponent* CompB = GetComponentFromActor(PairHeader.actors[1]);

	if (!CompA || !CompB) return;

	AActor* ActorA = CompA->GetOwner();
	AActor* ActorB = CompB->GetOwner();

	for (physx::PxU32 Index = 0; Index < Count; ++Index)
	{
		const physx::PxContactPair& Pair = Pairs[Index];

		if (!(Pair.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)) continue;

		FHitResult HitA;
		HitA.bHit = true;
		HitA.HitComponent = CompB;
		HitA.HitActor = ActorB;

		FHitResult HitB;
		HitB.bHit = true;
		HitB.HitComponent = CompA;
		HitB.HitActor = ActorA;

		physx::PxContactPairPoint ContactPoints[8];
		const physx::PxU32 ContactCount = Pair.extractContacts(ContactPoints, 8);

		FVector NormalImpulse = FVector::ZeroVector;

		if (ContactCount > 0)
		{
			const physx::PxContactPairPoint& Point = ContactPoints[0];

			HitA.WorldHitLocation = FromPxVec3(Point.position);
			HitA.WorldNormal = FromPxVec3(Point.normal);
			HitA.ImpactNormal = HitA.WorldNormal;

			HitB.WorldHitLocation = HitA.WorldHitLocation;
			HitB.WorldNormal = HitA.WorldNormal * -1.0f;
			HitB.ImpactNormal = HitB.WorldNormal;

			NormalImpulse = FromPxVec3(Point.impulse);
		}

		CompA->NotifyComponentHit(CompA, ActorB, CompB, NormalImpulse, HitA);
		CompB->NotifyComponentHit(CompB, ActorA, CompA, NormalImpulse * -1.0f, HitB);
	}
}

void FPhysicsEventCallback::onTrigger(physx::PxTriggerPair* Pairs, physx::PxU32 Count)
{
	for (physx::PxU32 Index = 0; Index < Count; ++Index)
	{
		const physx::PxTriggerPair& Pair = Pairs[Index];

		if (IsRemovedTriggerPair(Pair)) continue;

		UPrimitiveComponent* TriggerComp = GetComponentFromShape(Pair.triggerShape);
		UPrimitiveComponent* OtherComp = GetComponentFromShape(Pair.otherShape);

		if (!TriggerComp || !OtherComp) continue;
		if (!TriggerComp->GetGenerateOverlapEvents() || !OtherComp->GetGenerateOverlapEvents()) continue;

		AActor* TriggerActor = TriggerComp->GetOwner();
		AActor* OtherActor = OtherComp->GetOwner();

		FHitResult SweepResult;
		SweepResult.bHit = true;
		SweepResult.HitComponent = OtherComp;
		SweepResult.HitActor = OtherActor;

		if (Pair.status & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			TriggerComp->NotifyComponentBeginOverlap(TriggerComp, OtherActor, OtherComp,
				0, false, SweepResult);
			OtherComp->NotifyComponentBeginOverlap(OtherComp, TriggerActor, TriggerComp,
				0, false, SweepResult);
		}

		if (Pair.status & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
		{
			TriggerComp->NotifyComponentEndOverlap(TriggerComp, OtherActor, OtherComp, 0);
			OtherComp->NotifyComponentEndOverlap(OtherComp, TriggerActor, TriggerComp, 0);
		}
	}
}
