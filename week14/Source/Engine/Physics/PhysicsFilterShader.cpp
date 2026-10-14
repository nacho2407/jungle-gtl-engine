#include "Physics/PhysicsFilterShader.h"
#include "Physics/PhysicsFilterData.h"

#include "Core/Types/CollisionTypes.h"

namespace
{

	bool HasAny(physx::PxU32 Mask, physx::PxU32 Bits)
	{
		return (Mask & Bits) != 0;
	}

	bool IsQueryEnabled(const physx::PxFilterData& Data)
	{
		return HasAny(Data.word3, EPhysicsFilterFlags::QueryOnly | EPhysicsFilterFlags::QueryAndPhysics);
	}

	bool IsPhysicsEnabled(const physx::PxFilterData& Data)
	{
		return HasAny(Data.word3, EPhysicsFilterFlags::PhysicsOnly | EPhysicsFilterFlags::QueryAndPhysics);
	}

	bool CanGenerateOverlap(const physx::PxFilterData& A, const physx::PxFilterData& B)
	{
		return HasAny(A.word3, EPhysicsFilterFlags::GenerateOverlapEvents) &&
			HasAny(B.word3, EPhysicsFilterFlags::GenerateOverlapEvents);
	}

	uint16 GetSelfCollisionGroup(const physx::PxFilterData& Data)
	{
		return static_cast<uint16>((Data.word3 & EPhysicsFilterFlags::SelfCollisionGroupMask)
			>> EPhysicsFilterFlags::SelfCollisionGroupShift);
	}

	bool ShouldSuppressSelfCollision(const physx::PxFilterData& A, const physx::PxFilterData& B)
	{
		if (!HasAny(A.word3, EPhysicsFilterFlags::DisableSelfCollision) ||
			!HasAny(B.word3, EPhysicsFilterFlags::DisableSelfCollision))
		{
			return false;
		}

		const uint16 GroupA = GetSelfCollisionGroup(A);
		const uint16 GroupB = GetSelfCollisionGroup(B);
		return GroupA != 0 && GroupA == GroupB;
	}

	ECollisionResponse GetPairResponse(const physx::PxFilterData& A, const physx::PxFilterData& B)
	{
		const bool ABlocksB = HasAny(A.word1, B.word0);
		const bool BBlocksA = HasAny(B.word1, A.word0);

		if (ABlocksB && BBlocksA)
		{
			return ECollisionResponse::Block;
		}

		const bool AOverlapsB = HasAny(A.word2, B.word0);
		const bool BOverlapsA = HasAny(B.word2, A.word0);

		if (AOverlapsB || BOverlapsA)
		{
			return ECollisionResponse::Overlap;
		}

		return ECollisionResponse::Ignore;
	}

}

physx::PxFilterFlags PhysicsFilterShader(
	physx::PxFilterObjectAttributes Attributes0,
	physx::PxFilterData FilterData0,
	physx::PxFilterObjectAttributes Attributes1,
	physx::PxFilterData FilterData1,
	physx::PxPairFlags& PairFlags,
	const void* ConstantBlock,
	physx::PxU32 ConstantBlockSize)
{
	if (ShouldSuppressSelfCollision(FilterData0, FilterData1))
	{
		PairFlags = physx::PxPairFlags();
		return physx::PxFilterFlag::eSUPPRESS;
	}

	const ECollisionResponse Response = GetPairResponse(FilterData0, FilterData1);

	if (Response == ECollisionResponse::Ignore)
	{
		PairFlags = physx::PxPairFlags();
		return physx::PxFilterFlag::eSUPPRESS;
	}

	if (physx::PxFilterObjectIsTrigger(Attributes0) ||
		physx::PxFilterObjectIsTrigger(Attributes1) ||
		Response == ECollisionResponse::Overlap ||
		!IsPhysicsEnabled(FilterData0) ||
		!IsPhysicsEnabled(FilterData1))
	{
		if (!IsQueryEnabled(FilterData0) || !IsQueryEnabled(FilterData1))
		{
			PairFlags = physx::PxPairFlags();
			return physx::PxFilterFlag::eSUPPRESS;
		}

		PairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
		return physx::PxFilterFlag::eDEFAULT;
	}

	PairFlags = physx::PxPairFlag::eCONTACT_DEFAULT |
		physx::PxPairFlag::eNOTIFY_TOUCH_FOUND |
		physx::PxPairFlag::eNOTIFY_TOUCH_LOST |
		physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;

	return physx::PxFilterFlag::eDEFAULT;
}
