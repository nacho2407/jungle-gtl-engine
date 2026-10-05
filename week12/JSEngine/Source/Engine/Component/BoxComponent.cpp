#include "BoxComponent.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>

FVector UBoxComponent::GetScaledBoxExtent() const
{
	const FVector Scale = GetWorldScale();
	return FVector(
		std::abs(Extent.X * 0.5f * Scale.X),
		std::abs(Extent.Y * 0.5f * Scale.Y),
		std::abs(Extent.Z * 0.5f * Scale.Z));
}

bool UBoxComponent::FindSegmentBoxFirstHit(
	const FVector& StartWS,
	const FVector& EndWS,
	const FVector& ExpandedExtent,
	bool bFindInitialOverlaps,
	float& OutTime,
	FVector& OutNormalLocal,
	bool& bOutStartPenetrating) const
{
	constexpr float QueryTolerance = 1.e-6f;
	const FQuat Rotation = GetWorldTransform().GetRotation();
	const FVector CenterWS = GetWorldLocation();
	const FVector StartLocal = Rotation.Inverse().RotateVector(StartWS - CenterWS);
	const FVector EndLocal = Rotation.Inverse().RotateVector(EndWS - CenterWS);
	const FVector MoveLocal = EndLocal - StartLocal;

	const bool bStartsInside =
		std::abs(StartLocal.X) <= ExpandedExtent.X &&
		std::abs(StartLocal.Y) <= ExpandedExtent.Y &&
		std::abs(StartLocal.Z) <= ExpandedExtent.Z;
	if (bStartsInside)
	{
		if (!bFindInitialOverlaps)
		{
			return false;
		}

		int32 ClosestFaceAxis = 0;
		float ClosestFaceDistance = ExpandedExtent.X - std::abs(StartLocal.X);
		for (int32 Axis = 1; Axis < 3; ++Axis)
		{
			const float FaceDistance = ExpandedExtent[Axis] - std::abs(StartLocal[Axis]);
			if (FaceDistance < ClosestFaceDistance)
			{
				ClosestFaceAxis = Axis;
				ClosestFaceDistance = FaceDistance;
			}
		}

		float NormalSign = StartLocal[ClosestFaceAxis] >= 0.0f ? 1.0f : -1.0f;
		if (std::abs(StartLocal[ClosestFaceAxis]) <= QueryTolerance &&
			std::abs(MoveLocal[ClosestFaceAxis]) > QueryTolerance)
		{
			NormalSign = MoveLocal[ClosestFaceAxis] > 0.0f ? -1.0f : 1.0f;
		}

		OutTime = 0.0f;
		OutNormalLocal = FVector::ZeroVector;
		OutNormalLocal[ClosestFaceAxis] = NormalSign;
		bOutStartPenetrating = true;
		return true;
	}

	float EntryTime = 0.0f;
	float ExitTime = 1.0f;
	FVector EntryNormal = FVector::ZeroVector;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (std::abs(MoveLocal[Axis]) <= QueryTolerance)
		{
			if (StartLocal[Axis] < -ExpandedExtent[Axis] ||
				StartLocal[Axis] > ExpandedExtent[Axis])
			{
				return false;
			}
			continue;
		}

		const float InverseMove = 1.0f / MoveLocal[Axis];
		float FirstTime = (-ExpandedExtent[Axis] - StartLocal[Axis]) * InverseMove;
		float SecondTime = (ExpandedExtent[Axis] - StartLocal[Axis]) * InverseMove;
		FVector FirstNormal = FVector::ZeroVector;
		FVector SecondNormal = FVector::ZeroVector;
		FirstNormal[Axis] = -1.0f;
		SecondNormal[Axis] = 1.0f;
		if (FirstTime > SecondTime)
		{
			std::swap(FirstTime, SecondTime);
			std::swap(FirstNormal, SecondNormal);
		}

		if (FirstTime > EntryTime)
		{
			EntryTime = FirstTime;
			EntryNormal = FirstNormal;
		}
		ExitTime = std::min(ExitTime, SecondTime);
		if (EntryTime > ExitTime)
		{
			return false;
		}
	}

	if (EntryTime < 0.0f || EntryTime > 1.0f ||
		EntryNormal.SizeSquared() <= QueryTolerance * QueryTolerance)
	{
		return false;
	}

	OutTime = EntryTime;
	OutNormalLocal = EntryNormal;
	bOutStartPenetrating = false;
	return true;
}

void UBoxComponent::FillHitResult(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FVector& TargetExtent,
	float Time,
	const FVector& NormalLocal,
	bool bStartPenetrating) const
{
	const FQuat Rotation = GetWorldTransform().GetRotation();
	const FVector CenterWS = GetWorldLocation();
	const FVector HitCenterWS = StartWS + (EndWS - StartWS) * Time;
	const FVector HitCenterLocal = Rotation.Inverse().RotateVector(HitCenterWS - CenterWS);
	FVector ContactLocal(
		std::clamp(HitCenterLocal.X, -TargetExtent.X, TargetExtent.X),
		std::clamp(HitCenterLocal.Y, -TargetExtent.Y, TargetExtent.Y),
		std::clamp(HitCenterLocal.Z, -TargetExtent.Z, TargetExtent.Z));

	if (std::abs(NormalLocal.X) > 0.5f)
	{
		ContactLocal.X = NormalLocal.X > 0.0f ? TargetExtent.X : -TargetExtent.X;
	}
	else if (std::abs(NormalLocal.Y) > 0.5f)
	{
		ContactLocal.Y = NormalLocal.Y > 0.0f ? TargetExtent.Y : -TargetExtent.Y;
	}
	else
	{
		ContactLocal.Z = NormalLocal.Z > 0.0f ? TargetExtent.Z : -TargetExtent.Z;
	}

	const FVector NormalWS = Rotation.RotateVector(NormalLocal).GetSafeNormal();
	OutHit.HitComponent = const_cast<UBoxComponent*>(this);
	OutHit.Distance = std::sqrt((EndWS - StartWS).SizeSquared()) * Time;
	OutHit.Time = Time;
	OutHit.Location = CenterWS + Rotation.RotateVector(ContactLocal);
	OutHit.Normal = NormalWS;
	OutHit.bHit = true;
	OutHit.bStartPenetrating = bStartPenetrating;
}

bool UBoxComponent::LineTraceShape(
	FHitResult& OutHit,
	const FVector& StartWS,
	const FVector& EndWS,
	const FCollisionQueryParams& Params) const
{
	OutHit.Reset();
	const FVector TargetExtent = GetScaledBoxExtent();
	float HitTime = 1.0f;
	FVector HitNormalLocal = FVector::ZeroVector;
	bool bStartPenetrating = false;
	if (!FindSegmentBoxFirstHit(
		StartWS,
		EndWS,
		TargetExtent,
		Params.bFindInitialOverlaps,
		HitTime,
		HitNormalLocal,
		bStartPenetrating))
	{
		return false;
	}

	FillHitResult(
		OutHit,
		StartWS,
		EndWS,
		TargetExtent,
		HitTime,
		HitNormalLocal,
		bStartPenetrating);
	return true;
}

bool UBoxComponent::SweepShape(
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

	const FVector TargetExtent = GetScaledBoxExtent();
	const FVector ExpandedExtent =
		TargetExtent + FVector(CollisionShape.SphereRadius, CollisionShape.SphereRadius, CollisionShape.SphereRadius);
	float HitTime = 1.0f;
	FVector HitNormalLocal = FVector::ZeroVector;
	bool bStartPenetrating = false;

	// 구를 움직여 box와 정확히 검사하려면 rounded box 처리 필요
	// 이번 범위에서는 extent를 반지름만큼 늘리는 보수 근사만 사용
	// corner 근처에서는 실제보다 먼저 충돌했다고 판단할 수 있음
	if (!FindSegmentBoxFirstHit(
		StartWS,
		EndWS,
		ExpandedExtent,
		Params.bFindInitialOverlaps,
		HitTime,
		HitNormalLocal,
		bStartPenetrating))
	{
		return false;
	}

	FillHitResult(
		OutHit,
		StartWS,
		EndWS,
		TargetExtent,
		HitTime,
		HitNormalLocal,
		bStartPenetrating);
	return true;
}

bool UBoxComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

EPrimitiveType UBoxComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Box;
}
