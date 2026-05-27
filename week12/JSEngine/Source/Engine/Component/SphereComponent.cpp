#include "SphereComponent.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>

FVector USphereComponent::MakeHitNormal(const FVector& CenterToQuery, const FVector& Move)
{
		constexpr float QueryTolerance = 1.e-6f;
		if (CenterToQuery.SizeSquared() > QueryTolerance * QueryTolerance)
		{
			return CenterToQuery.GetSafeNormal();
		}

		// 중심에 정확히 놓인 시작 겹침은 normal이 하나로 정해지지 않는다.
		// 이동 방향 반대를 고르면 적어도 들어오던 방향으로 다시 밀어내지 않는다.
		if (Move.SizeSquared() > QueryTolerance * QueryTolerance)
		{
			return (-Move).GetSafeNormal();
		}

		return FVector::UpVector;
}

bool USphereComponent::FindSegmentSphereFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& TargetCenterWS,
		float ExpandedRadius,
		bool bFindInitialOverlaps,
		float& OutTime,
		FVector& OutNormal,
		bool& bOutStartPenetrating)
{
		constexpr float QueryTolerance = 1.e-6f;
		const FVector Move = EndWS - StartWS;
		const FVector CenterToStart = StartWS - TargetCenterWS;
		const float RadiusSquared = ExpandedRadius * ExpandedRadius;

		if (CenterToStart.SizeSquared() <= RadiusSquared)
		{
			if (!bFindInitialOverlaps)
			{
				return false;
			}

			OutTime = 0.0f;
			OutNormal = MakeHitNormal(CenterToStart, Move);
			bOutStartPenetrating = true;
			return true;
		}

		const float MoveLengthSquared = Move.SizeSquared();
		if (MoveLengthSquared <= QueryTolerance * QueryTolerance)
		{
			return false;
		}

		// 움직이는 점과 구의 거리를 제곱하면 t에 대한 이차식이 된다.
		// 두 해 중 작은 해가 이번 이동에서 구 표면에 처음 닿는 시각이다.
		const float HalfB = FVector::DotProduct(CenterToStart, Move);
		const float C = CenterToStart.SizeSquared() - RadiusSquared;
		const float Discriminant = HalfB * HalfB - MoveLengthSquared * C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		const float Time = (-HalfB - std::sqrt(std::max(Discriminant, 0.0f))) / MoveLengthSquared;
		if (Time < 0.0f || Time > 1.0f)
		{
			return false;
		}

		const FVector QueryCenterAtHit = StartWS + Move * Time;
		OutTime = Time;
		OutNormal = MakeHitNormal(QueryCenterAtHit - TargetCenterWS, Move);
		bOutStartPenetrating = false;
		return true;
}

void USphereComponent::FillHitResult(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& TargetCenterWS,
		float TargetRadius,
		float Time,
		const FVector& Normal,
		bool bStartPenetrating) const
{
		const FVector Move = EndWS - StartWS;
		OutHit.HitComponent = const_cast<USphereComponent*>(this);
		OutHit.Distance = std::sqrt(Move.SizeSquared()) * Time;
		OutHit.Time = Time;
		// Sweep에서 맞은 중심은 따로 계산한다. event에 남기는 위치는 target 표면이다.
		OutHit.Location = TargetCenterWS + Normal * TargetRadius;
		OutHit.Normal = Normal;
		OutHit.bHit = true;
		OutHit.bStartPenetrating = bStartPenetrating;
}



void USphereComponent::PostDuplicate(UObject* Original)
{
	UShapeComponent::PostDuplicate(Original);

	USphereComponent* SphereComp = Cast<USphereComponent>(Original);
	SphereRadius = SphereComp->SphereRadius;
}

float USphereComponent::GetScaledSphereRadius() const
{
	const FVector Scale = GetWorldScale();
	const float MaxAbsoluteScale = std::max(
		std::abs(Scale.X),
		std::max(std::abs(Scale.Y), std::abs(Scale.Z)));

	// 구는 한 축만 크게 늘어나도 그 축까지 감싸는 반지름으로 query해야 한다.
	// bounds와 narrow phase가 다른 크기를 쓰면 broad phase가 실제 hit를 놓친다.
	return SphereRadius * MaxAbsoluteScale;
}

bool USphereComponent::LineTraceShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionQueryParams& Params) const
{
	OutHit.Reset();

	float HitTime = 1.0f;
	FVector HitNormal = FVector::ZeroVector;
	bool bStartPenetrating = false;
	const FVector TargetCenterWS = GetWorldLocation();
	const float TargetRadius = GetScaledSphereRadius();
	if (!FindSegmentSphereFirstHit(
		StartWS,
		EndWS,
		TargetCenterWS,
		TargetRadius,
		Params.bFindInitialOverlaps,
		HitTime,
		HitNormal,
		bStartPenetrating))
	{
		return false;
	}

	FillHitResult(
		OutHit,
		StartWS,
		EndWS,
		TargetCenterWS,
		TargetRadius,
		HitTime,
		HitNormal,
		bStartPenetrating);
	return true;
}

bool USphereComponent::SweepShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionShape& CollisionShape,
	const FCollisionQueryParams& Params) const
{
	if (CollisionShape.IsNearlyZero())
	{
		return LineTraceShape(OutHit, StartWS, EndWS, Params);
	}

	OutHit.Reset();
	if (!CollisionShape.IsSphere())
	{
		return false;
	}

	const FVector TargetCenterWS = GetWorldLocation();
	const float TargetRadius = GetScaledSphereRadius();
	const float ExpandedRadius = TargetRadius + CollisionShape.SphereRadius;
	float HitTime = 1.0f;
	FVector HitNormal = FVector::ZeroVector;
	bool bStartPenetrating = false;

	// 움직이는 구는 반지름만큼 target 구를 키운 뒤 중심점 하나만 움직여 검사한다.
	if (!FindSegmentSphereFirstHit(
		StartWS,
		EndWS,
		TargetCenterWS,
		ExpandedRadius,
		Params.bFindInitialOverlaps,
		HitTime,
		HitNormal,
		bStartPenetrating))
	{
		return false;
	}

	FillHitResult(
		OutHit,
		StartWS,
		EndWS,
		TargetCenterWS,
		TargetRadius,
		HitTime,
		HitNormal,
		bStartPenetrating);
	return true;
}


void USphereComponent::UpdateWorldAABB() const
{
	const FVector Center = GetWorldLocation();

	const float ScaledRadius = GetScaledSphereRadius();
	WorldAABB.Min = Center - FVector(ScaledRadius, ScaledRadius, ScaledRadius);
	WorldAABB.Max = Center + FVector(ScaledRadius, ScaledRadius, ScaledRadius);
}

bool USphereComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

EPrimitiveType USphereComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Sphere;
}
