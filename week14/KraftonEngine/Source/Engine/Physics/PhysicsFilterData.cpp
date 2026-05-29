#include "Physics/PhysicsFilterData.h"

#include "Component/PrimitiveComponent.h"
#include "Core/Types/CollisionTypes.h"

physx::PxFilterData MakeFilterData(const UPrimitiveComponent& Component)
{
	physx::PxFilterData Data;

	Data.word0 = ObjectTypeBit(Component.GetCollisionObjectType());

	Data.word1 = 0;
	Data.word2 = 0;

	for (int32 Index = 0; Index < static_cast<int32>(ECollisionChannel::ActiveCount); ++Index)
	{
		const ECollisionChannel Channel = static_cast<ECollisionChannel>(Index);
		const ECollisionResponse Response = Component.GetCollisionResponseToChannel(Channel);
		const uint32 Bit = ObjectTypeBit(Channel);

		if (Response == ECollisionResponse::Block)
		{
			Data.word1 |= Bit;
		}
		else if (Response == ECollisionResponse::Overlap)
		{
			Data.word2 |= Bit;
		}
	}

	Data.word3 = 0;

	switch (Component.GetCollisionEnabled())
	{
	case ECollisionEnabled::QueryOnly:
		Data.word3 |= EPhysicsFilterFlags::QueryOnly;
		break;
	case ECollisionEnabled::PhysicsOnly:
		Data.word3 |= EPhysicsFilterFlags::PhysicsOnly;
		break;
	case ECollisionEnabled::QueryAndPhysics:
		Data.word3 |= EPhysicsFilterFlags::QueryAndPhysics;
		break;
	default:
		break;
	}

	if (Component.GetGenerateOverlapEvents())
	{
		Data.word3 |= EPhysicsFilterFlags::GenerateOverlapEvents;
	}

	return Data;
}