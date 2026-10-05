#include "CapsuleComponent.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>


FVector UCapsuleComponent::ClosestPointOnAxis(
		const FVector& Point,
		const FVector& Center,
		const FVector& Axis,
		float HalfHeight)
{
		const float AxisDistance = FVector::DotProduct(Point - Center, Axis);
		const float ClampedDistance = std::clamp(AxisDistance, -HalfHeight, HalfHeight);
		return Center + Axis * ClampedDistance;
}

FVector UCapsuleComponent::MakeHitNormal(
		const FVector& AxisPointToQuery,
		const FVector& Move,
		const FVector& Axis)
{
		constexpr float QueryTolerance = 1.e-6f;
		if (AxisPointToQuery.SizeSquared() > QueryTolerance * QueryTolerance)
		{
			return AxisPointToQuery.GetSafeNormal();
		}

		// 캡슐 중심선 위에서 시작하면 바깥 방향이 하나로 정해지지 않는다.
		// 축과 수직인 방향을 골라야 계산한 접촉점이 캡슐 표면 위에 남는다.
		const FVector Reference =
			std::abs(FVector::DotProduct(Axis, FVector::UpVector)) < 0.99f
			? FVector::UpVector
			: FVector(1.0f, 0.0f, 0.0f);
		FVector Normal = FVector::CrossProduct(Axis, Reference).GetSafeNormal();
		if (Move.SizeSquared() > QueryTolerance * QueryTolerance &&
			FVector::DotProduct(Normal, Move) > 0.0f)
		{
			Normal = -Normal;
		}
		return Normal;
}

bool UCapsuleComponent::FindSegmentSphereFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& SphereCenterWS,
		float Radius,
		float& OutTime)
{
		constexpr float QueryTolerance = 1.e-6f;
		const FVector Move = EndWS - StartWS;
		const FVector CenterToStart = StartWS - SphereCenterWS;
		const float MoveLengthSquared = Move.SizeSquared();
		if (MoveLengthSquared <= QueryTolerance * QueryTolerance)
		{
			return false;
		}

		const float HalfB = FVector::DotProduct(CenterToStart, Move);
		const float C = CenterToStart.SizeSquared() - Radius * Radius;
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

		OutTime = Time;
		return true;
}

bool UCapsuleComponent::FindSegmentCapsuleFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& CapsuleCenterWS,
		const FVector& CapsuleAxis,
		float HalfHeight,
		float ExpandedRadius,
		bool bFindInitialOverlaps,
		float& OutTime,
		FVector& OutNormal,
		bool& bOutStartPenetrating)
{
		constexpr float QueryTolerance = 1.e-6f;
		const FVector Move = EndWS - StartWS;
		const FVector ClosestStartAxisPoint =
			ClosestPointOnAxis(StartWS, CapsuleCenterWS, CapsuleAxis, HalfHeight);
		const FVector AxisToStart = StartWS - ClosestStartAxisPoint;
		if (AxisToStart.SizeSquared() <= ExpandedRadius * ExpandedRadius)
		{
			if (!bFindInitialOverlaps)
			{
				return false;
			}

			OutTime = 0.0f;
			OutNormal = MakeHitNormal(AxisToStart, Move, CapsuleAxis);
			bOutStartPenetrating = true;
			return true;
		}

		if (Move.SizeSquared() <= QueryTolerance * QueryTolerance)
		{
			return false;
		}

		float BestTime = 2.0f;
		bool bFoundTime = false;
		auto KeepEarlierTime = [&BestTime, &bFoundTime](float Time)
		{
			if (Time >= 0.0f && Time <= 1.0f && (!bFoundTime || Time < BestTime))
			{
				BestTime = Time;
				bFoundTime = true;
			}
		};

		// 최근접 위치는 최초 충돌 시각이 아닐 수 있다.
		// collision response에는 처음 닿은 시각이 필요해서 옆면과 양 끝 구의 후보 시간을 따로 비교한다.
		const FVector CenterToStart = StartWS - CapsuleCenterWS;
		const float StartAlongAxis = FVector::DotProduct(CenterToStart, CapsuleAxis);
		const float MoveAlongAxis = FVector::DotProduct(Move, CapsuleAxis);
		const FVector RadialStart = CenterToStart - CapsuleAxis * StartAlongAxis;
		const FVector RadialMove = Move - CapsuleAxis * MoveAlongAxis;
		const float A = RadialMove.SizeSquared();
		const float B = 2.0f * FVector::DotProduct(RadialStart, RadialMove);
		const float C = RadialStart.SizeSquared() - ExpandedRadius * ExpandedRadius;
		if (A > QueryTolerance * QueryTolerance)
		{
			const float Discriminant = B * B - 4.0f * A * C;
			if (Discriminant >= 0.0f)
			{
				const float SideTime =
					(-B - std::sqrt(std::max(Discriminant, 0.0f))) / (2.0f * A);
				const float SideAxisDistance = StartAlongAxis + MoveAlongAxis * SideTime;
				if (SideTime >= 0.0f && SideTime <= 1.0f &&
					SideAxisDistance >= -HalfHeight && SideAxisDistance <= HalfHeight)
				{
					KeepEarlierTime(SideTime);
				}
			}
		}

		const FVector TopCenter = CapsuleCenterWS + CapsuleAxis * HalfHeight;
		const FVector BottomCenter = CapsuleCenterWS - CapsuleAxis * HalfHeight;
		float CapTime = 0.0f;
		if (FindSegmentSphereFirstHit(StartWS, EndWS, TopCenter, ExpandedRadius, CapTime))
		{
			KeepEarlierTime(CapTime);
		}
		if (FindSegmentSphereFirstHit(StartWS, EndWS, BottomCenter, ExpandedRadius, CapTime))
		{
			KeepEarlierTime(CapTime);
		}

		if (!bFoundTime)
		{
			return false;
		}

		const FVector QueryCenterAtHit = StartWS + Move * BestTime;
		const FVector ClosestAxisPoint =
			ClosestPointOnAxis(QueryCenterAtHit, CapsuleCenterWS, CapsuleAxis, HalfHeight);
		OutTime = BestTime;
		OutNormal = MakeHitNormal(QueryCenterAtHit - ClosestAxisPoint, Move, CapsuleAxis);
		bOutStartPenetrating = false;
		return true;
}

void UCapsuleComponent::FillHitResult(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& CapsuleCenterWS,
		const FVector& CapsuleAxis,
		float CapsuleHalfHeight,
		float TargetRadius,
		float Time,
		const FVector& Normal,
		bool bStartPenetrating) const
{
		const FVector Move = EndWS - StartWS;
		const FVector QueryCenterAtHit = StartWS + Move * Time;
		const FVector ClosestAxisPoint =
			ClosestPointOnAxis(QueryCenterAtHit, CapsuleCenterWS, CapsuleAxis, CapsuleHalfHeight);

		OutHit.HitComponent = const_cast<UCapsuleComponent*>(this);
		OutHit.Distance = std::sqrt(Move.SizeSquared()) * Time;
		OutHit.Time = Time;
		// Expanded capsule은 hit 시각만 계산한다. event 위치는 원래 target 표면에 둔다.
		OutHit.Location = ClosestAxisPoint + Normal * TargetRadius;
		OutHit.Normal = Normal;
		OutHit.bHit = true;
		OutHit.bStartPenetrating = bStartPenetrating;
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
	const FVector Scale = GetWorldScale();
	return CapsuleHalfHeight * std::abs(Scale.Z);
}

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
	const FVector Scale = GetWorldScale();
	const float RadialScale = std::max(std::abs(Scale.X), std::abs(Scale.Y));

	// 캡슐의 길이는 local Z가 정하고, 굵기는 local X/Y가 정한다.
	// bounds와 narrow phase 모두 이 규칙을 써야 scale이 바뀌어도 모양이 맞는다.
	return CapsuleRadius * RadialScale;
}

bool UCapsuleComponent::LineTraceShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionQueryParams& Params) const
{
	OutHit.Reset();

	const FTransform Transform = GetWorldTransform();
	const FVector Center = Transform.GetLocation();
	const FVector Axis = Transform.GetUnitAxis(EAxis::Z);
	const float HalfHeight = GetScaledCapsuleHalfHeight();
	const float Radius = GetScaledCapsuleRadius();
	float HitTime = 1.0f;
	FVector HitNormal = FVector::ZeroVector;
	bool bStartPenetrating = false;
	if (!FindSegmentCapsuleFirstHit(
		StartWS,
		EndWS,
		Center,
		Axis,
		HalfHeight,
		Radius,
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
		Center,
		Axis,
		HalfHeight,
		Radius,
		HitTime,
		HitNormal,
		bStartPenetrating);
	return true;
}

bool UCapsuleComponent::SweepShape(
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

	const FTransform Transform = GetWorldTransform();
	const FVector Center = Transform.GetLocation();
	const FVector Axis = Transform.GetUnitAxis(EAxis::Z);
	const float Radius = GetScaledCapsuleRadius();
	const float HalfHeight = GetScaledCapsuleHalfHeight();
	const float ExpandedRadius = Radius + CollisionShape.SphereRadius;
	float HitTime = 1.0f;
	FVector HitNormal = FVector::ZeroVector;
	bool bStartPenetrating = false;

	// 움직이는 구는 캡슐의 반지름을 늘려서 중심점 하나의 이동으로 바꿔 검사한다.
	if (!FindSegmentCapsuleFirstHit(
		StartWS,
		EndWS,
		Center,
		Axis,
		HalfHeight,
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
		Center,
		Axis,
		HalfHeight,
		Radius,
		HitTime,
		HitNormal,
		bStartPenetrating);
	return true;
}

void UCapsuleComponent::UpdateWorldAABB() const
{
	FTransform T = GetWorldTransform();

	FVector Center = T.GetLocation();
	FVector Axis = T.GetUnitAxis(EAxis::Z);

	float HalfHeight = GetScaledCapsuleHalfHeight();
	float Radius = GetScaledCapsuleRadius();

	FVector A = Center + Axis * HalfHeight;
	FVector B = Center - Axis * HalfHeight;

	FVector Min(
		std::min(A.X, B.X),
		std::min(A.Y, B.Y),
		std::min(A.Z, B.Z));

	FVector Max(
		std::max(A.X, B.X),
		std::max(A.Y, B.Y),
		std::max(A.Z, B.Z));

	// radius expansion (AABB padding)
	Min -= FVector(Radius, Radius, Radius);
	Max += FVector(Radius, Radius, Radius);

	WorldAABB.Min = Min;
	WorldAABB.Max = Max;
}

bool UCapsuleComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

EPrimitiveType UCapsuleComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Capsule;
}
