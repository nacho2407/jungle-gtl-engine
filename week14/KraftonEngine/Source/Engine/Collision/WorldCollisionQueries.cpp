#include "Collision/WorldCollisionQueries.h"

#include "Core/Types/CoreTypes.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "Collision/Math/CollisionMath.h"
#include "GameFramework/World.h"

namespace
{
	// AABB slab 테스트 + closest-hit 갱신. 어떤 컴포넌트가 통과되는지 정하는 predicate 만
	// 호출자가 주입. Raycast / RaycastByObjectTypes 가 같은 기하 코드를 공유한다.
	template<typename FPredicate>
	bool NativeRaycastImpl(
		const UWorld& World,
		const FVector& Start, const FVector& Dir, float MaxDist,
		const AActor* IgnoreActor,
		FPredicate AcceptComponent,
		FHitResult& OutHit)
	{
		FVector InvDir;
		InvDir.X = (Dir.X != 0.0f) ? (1.0f / Dir.X) : 1e30f;
		InvDir.Y = (Dir.Y != 0.0f) ? (1.0f / Dir.Y) : 1e30f;
		InvDir.Z = (Dir.Z != 0.0f) ? (1.0f / Dir.Z) : 1e30f;

		float ClosestDist = MaxDist;
		bool bFound = false;

		for (AActor* Actor : World.GetActors())
		{
			if (!Actor) continue;
			if (IgnoreActor && Actor == IgnoreActor) continue;

			for (UPrimitiveComponent* Comp : Actor->GetPrimitiveComponents())
			{
				if (!Comp) continue;
				if (!Comp->IsQueryCollisionEnabled()) continue;
				if (!AcceptComponent(Comp)) continue;

				FBoundingBox Box = Comp->GetWorldBoundingBox();

				float tMin = (Box.Min.X - Start.X) * InvDir.X;
				float tMax = (Box.Max.X - Start.X) * InvDir.X;
				if (tMin > tMax) { float tmp = tMin; tMin = tMax; tMax = tmp; }

				float tyMin = (Box.Min.Y - Start.Y) * InvDir.Y;
				float tyMax = (Box.Max.Y - Start.Y) * InvDir.Y;
				if (tyMin > tyMax) { float tmp = tyMin; tyMin = tyMax; tyMax = tmp; }

				if ((tMin > tyMax) || (tyMin > tMax)) continue;
				if (tyMin > tMin) tMin = tyMin;
				if (tyMax < tMax) tMax = tyMax;

				float tzMin = (Box.Min.Z - Start.Z) * InvDir.Z;
				float tzMax = (Box.Max.Z - Start.Z) * InvDir.Z;
				if (tzMin > tzMax) { float tmp = tzMin; tzMin = tzMax; tzMax = tmp; }

				if ((tMin > tzMax) || (tzMin > tMax)) continue;
				if (tzMin > tMin) tMin = tzMin;

				if (tMin < 0.0f) tMin = 0.0f;
				if (tMin >= ClosestDist) continue;

				ClosestDist = tMin;
				bFound = true;

				OutHit.bHit = true;
				OutHit.Distance = tMin;
				OutHit.HitComponent = Comp;
				OutHit.HitActor = Comp->GetOwner();
				OutHit.WorldHitLocation = Start + Dir * tMin;

			}
		}

		return bFound;
	}
}

bool FWorldCollisionQueries::Raycast(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	// Channel filter: 응답이 TraceChannel에 대해 Block이 아니면 skip
	// (overlap/ignore 응답인 trigger volume 등은 raycast 결과에서 제외)
	return NativeRaycastImpl(World, Start, Dir, MaxDist, IgnoreActor,
		[TraceChannel](UPrimitiveComponent* Comp)
	{
		return Comp->GetCollisionResponseToChannel(TraceChannel) == ECollisionResponse::Block;
	}, OutHit);
}

bool FWorldCollisionQueries::RaycastByObjectTypes(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	uint32 ObjectTypeMask, const AActor* IgnoreActor)
{
	if (ObjectTypeMask == 0) return false;

	// ObjectType 자체를 마스크로 필터. 응답은 보지 않음.
	// Trigger 류 (NoCollision / query 비활성) 는 query 의미상 hit 후보가 아니므로 제외.
	return NativeRaycastImpl(World, Start, Dir, MaxDist, IgnoreActor,
		[ObjectTypeMask](UPrimitiveComponent* Comp)
	{
		if (!Comp->IsQueryCollisionEnabled()) return false;
		const uint32 Bit = 1u << static_cast<uint32>(Comp->GetCollisionObjectType());
		return (Bit & ObjectTypeMask) != 0;
	}, OutHit);
}

bool FWorldCollisionQueries::SphereSweepShapeComponents(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, float Radius,
	FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	if (MaxDist <= 0.0f || Radius < 0.0f)
	{
		return false;
	}

	float ClosestDist = MaxDist;
	bool bFound = false;

	for (AActor* Actor : World.GetActors())
	{
		if (!Actor) continue;
		if (IgnoreActor && Actor == IgnoreActor) continue;

		for (UPrimitiveComponent* Comp : Actor->GetPrimitiveComponents())
		{
			if (!Comp) continue;
			UShapeComponent* Shape = Cast<UShapeComponent>(Comp);
			if (!Shape) continue;
			if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
			if (Comp->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) continue;

			FHitResult CandidateHit;
			if (!FCollisionMath::SweepSphereShapeComponent(Start, Dir, MaxDist, Radius, Shape, CandidateHit))
			{
				continue;
			}
			if (CandidateHit.Distance >= ClosestDist)
			{
				continue;
			}

			ClosestDist = CandidateHit.Distance;
			bFound = true;
			OutHit = CandidateHit;
		}
	}

	return bFound;
}
