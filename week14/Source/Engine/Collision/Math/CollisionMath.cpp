#include "Collision/Math/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/CurvedWallRunColliderComponent.h"
#include "Math/Quat.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Epsilon = 1.0e-6f;

	float ClampFloat(float Value, float Min, float Max)
	{
		return (std::max)(Min, (std::min)(Value, Max));
	}

	FVector SafeNormal(const FVector& Vector, const FVector& Fallback = FVector::UpVector)
	{
		FVector Result = Vector;
		if (Result.Length() <= 1.0e-4f)
		{
			Result = Fallback;
		}
		Result.Normalize();
		return Result;
	}

	struct FOrientedBox
	{
		FVector Center = FVector::ZeroVector;
		FVector Axis[3] = { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector };
		FVector Extent = FVector::OneVector;
	};

	struct FCapsuleSegment
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		float Radius = 0.0f;
	};

	FOrientedBox MakeOrientedBox(const UBoxComponent* Box)
	{
		FOrientedBox Result;
		Result.Center = Box->GetWorldLocation();
		Result.Axis[0] = SafeNormal(Box->GetForwardVector(), FVector::XAxisVector);
		Result.Axis[1] = SafeNormal(Box->GetRightVector(), FVector::YAxisVector);
		Result.Axis[2] = SafeNormal(Box->GetUpVector(), FVector::ZAxisVector);
		Result.Extent = Box->GetScaledBoxExtent();
		return Result;
	}

	FCapsuleSegment MakeCapsuleSegment(const UCapsuleComponent* Capsule)
	{
		const FVector Up = SafeNormal(Capsule->GetUpVector(), FVector::ZAxisVector);
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float SegmentHalfLength = (std::max)(0.0f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
		const FVector Center = Capsule->GetWorldLocation();

		FCapsuleSegment Result;
		Result.A = Center - Up * SegmentHalfLength;
		Result.B = Center + Up * SegmentHalfLength;
		Result.Radius = Radius;
		return Result;
	}

	FVector ClosestPointOnSegment(const FVector& Point, const FVector& A, const FVector& B)
	{
		const FVector AB = B - A;
		const float Denom = AB.Dot(AB);
		if (Denom <= Epsilon)
		{
			return A;
		}

		const float T = ClampFloat((Point - A).Dot(AB) / Denom, 0.0f, 1.0f);
		return A + AB * T;
	}

	void ClosestPointsOnSegments(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2,
		FVector& OutC1, FVector& OutC2)
	{
		const FVector D1 = Q1 - P1;
		const FVector D2 = Q2 - P2;
		const FVector R = P1 - P2;
		const float A = D1.Dot(D1);
		const float E = D2.Dot(D2);
		const float F = D2.Dot(R);

		float S = 0.0f;
		float T = 0.0f;

		if (A <= Epsilon && E <= Epsilon)
		{
			OutC1 = P1;
			OutC2 = P2;
			return;
		}

		if (A <= Epsilon)
		{
			T = ClampFloat(F / E, 0.0f, 1.0f);
		}
		else
		{
			const float C = D1.Dot(R);
			if (E <= Epsilon)
			{
				S = ClampFloat(-C / A, 0.0f, 1.0f);
			}
			else
			{
				const float B = D1.Dot(D2);
				const float Denom = A * E - B * B;
				if (Denom != 0.0f)
				{
					S = ClampFloat((B * F - C * E) / Denom, 0.0f, 1.0f);
				}

				T = (B * S + F) / E;
				if (T < 0.0f)
				{
					T = 0.0f;
					S = ClampFloat(-C / A, 0.0f, 1.0f);
				}
				else if (T > 1.0f)
				{
					T = 1.0f;
					S = ClampFloat((B - C) / A, 0.0f, 1.0f);
				}
			}
		}

		OutC1 = P1 + D1 * S;
		OutC2 = P2 + D2 * T;
	}

	FVector ClosestPointOnOBB(const FOrientedBox& Box, const FVector& Point)
	{
		FVector Result = Box.Center;
		const FVector Delta = Point - Box.Center;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Distance = ClampFloat(Delta.Dot(Box.Axis[AxisIndex]),
				-Box.Extent.Data[AxisIndex], Box.Extent.Data[AxisIndex]);
			Result += Box.Axis[AxisIndex] * Distance;
		}
		return Result;
	}

	bool PointInsideOBB(const FOrientedBox& Box, const FVector& Point)
	{
		const FVector Delta = Point - Box.Center;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			if (std::abs(Delta.Dot(Box.Axis[AxisIndex])) > Box.Extent.Data[AxisIndex])
			{
				return false;
			}
		}
		return true;
	}

	FVector ClosestPointOnOBBWhenInside(const FOrientedBox& Box, const FVector& Point, float& OutFaceDistance)
	{
		const FVector Delta = Point - Box.Center;
		float MinDistance = FLT_MAX;
		int32 BestAxis = 0;
		float BestSign = 1.0f;

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Coord = Delta.Dot(Box.Axis[AxisIndex]);
			const float FaceDistance = Box.Extent.Data[AxisIndex] - std::abs(Coord);
			if (FaceDistance < MinDistance)
			{
				MinDistance = FaceDistance;
				BestAxis = AxisIndex;
				BestSign = Coord >= 0.0f ? 1.0f : -1.0f;
			}
		}

		OutFaceDistance = MinDistance;
		return Point + Box.Axis[BestAxis] * (MinDistance * BestSign);
	}

	bool OBBvsSphere(const FOrientedBox& Box, const FVector& SphereCenter, float SphereRadius,
		FVector& OutNormal, float& OutDepth)
	{
		const FVector Closest = ClosestPointOnOBB(Box, SphereCenter);
		FVector Delta = SphereCenter - Closest;
		const float DistSq = Delta.Dot(Delta);

		if (DistSq > SphereRadius * SphereRadius)
		{
			return false;
		}

		const float Dist = std::sqrt((std::max)(0.0f, DistSq));
		if (Dist > Epsilon)
		{
			OutNormal = Delta * (1.0f / Dist);
			OutDepth = SphereRadius - Dist;
			return true;
		}

		float FaceDistance = 0.0f;
		const FVector FacePoint = ClosestPointOnOBBWhenInside(Box, SphereCenter, FaceDistance);
		OutNormal = SafeNormal(SphereCenter - FacePoint, SphereCenter - Box.Center);
		OutDepth = SphereRadius + FaceDistance;
		return true;
	}

	bool OBBvsOBB(const FOrientedBox& A, const FOrientedBox& B, FVector& OutNormal, float& OutDepth)
	{
		FVector Axes[15];
		int32 AxisCount = 0;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Axes[AxisCount++] = A.Axis[Index];
			Axes[AxisCount++] = B.Axis[Index];
		}
		for (int32 AIndex = 0; AIndex < 3; ++AIndex)
		{
			for (int32 BIndex = 0; BIndex < 3; ++BIndex)
			{
				const FVector Cross = A.Axis[AIndex].Cross(B.Axis[BIndex]);
				if (Cross.Dot(Cross) > 1.0e-8f)
				{
					Axes[AxisCount++] = SafeNormal(Cross);
				}
			}
		}

		const FVector CenterDelta = B.Center - A.Center;
		float MinOverlap = FLT_MAX;
		FVector BestAxis = FVector::UpVector;

		for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
		{
			FVector Axis = SafeNormal(Axes[AxisIndex]);
			const float RadiusA =
				A.Extent.X * std::abs(A.Axis[0].Dot(Axis)) +
				A.Extent.Y * std::abs(A.Axis[1].Dot(Axis)) +
				A.Extent.Z * std::abs(A.Axis[2].Dot(Axis));
			const float RadiusB =
				B.Extent.X * std::abs(B.Axis[0].Dot(Axis)) +
				B.Extent.Y * std::abs(B.Axis[1].Dot(Axis)) +
				B.Extent.Z * std::abs(B.Axis[2].Dot(Axis));
			const float Distance = std::abs(CenterDelta.Dot(Axis));
			const float Overlap = RadiusA + RadiusB - Distance;
			if (Overlap < 0.0f)
			{
				return false;
			}

			if (Overlap < MinOverlap)
			{
				MinOverlap = Overlap;
				BestAxis = Axis;
			}
		}

		if (CenterDelta.Dot(BestAxis) < 0.0f)
		{
			BestAxis *= -1.0f;
		}

		OutNormal = SafeNormal(BestAxis);
		OutDepth = MinOverlap;
		return true;
	}

	float DistanceSqPointOBB(const FOrientedBox& Box, const FVector& Point, FVector& OutClosest)
	{
		OutClosest = ClosestPointOnOBB(Box, Point);
		const FVector Delta = Point - OutClosest;
		return Delta.Dot(Delta);
	}

	bool OBBvsCapsule(const FOrientedBox& Box, const FCapsuleSegment& Capsule, FVector& OutNormal, float& OutDepth)
	{
		float BestT = 0.0f;
		float Lo = 0.0f;
		float Hi = 1.0f;
		const FVector Segment = Capsule.B - Capsule.A;

		for (int32 Iter = 0; Iter < 32; ++Iter)
		{
			const float T1 = Lo + (Hi - Lo) / 3.0f;
			const float T2 = Hi - (Hi - Lo) / 3.0f;
			FVector Closest1;
			FVector Closest2;
			const float D1 = DistanceSqPointOBB(Box, Capsule.A + Segment * T1, Closest1);
			const float D2 = DistanceSqPointOBB(Box, Capsule.A + Segment * T2, Closest2);
			if (D1 < D2)
			{
				Hi = T2;
			}
			else
			{
				Lo = T1;
			}
		}

		BestT = (Lo + Hi) * 0.5f;
		const FVector CapsulePoint = Capsule.A + Segment * BestT;
		FVector BoxPoint;
		const float DistSq = DistanceSqPointOBB(Box, CapsulePoint, BoxPoint);
		if (DistSq > Capsule.Radius * Capsule.Radius)
		{
			return false;
		}

		const float Dist = std::sqrt((std::max)(0.0f, DistSq));
		if (Dist > Epsilon)
		{
			OutNormal = (CapsulePoint - BoxPoint) * (1.0f / Dist);
			OutDepth = Capsule.Radius - Dist;
			return true;
		}

		float FaceDistance = 0.0f;
		const FVector FacePoint = ClosestPointOnOBBWhenInside(Box, CapsulePoint, FaceDistance);
		OutNormal = SafeNormal(CapsulePoint - FacePoint, CapsulePoint - Box.Center);
		OutDepth = Capsule.Radius + FaceDistance;
		return true;
	}

	bool SphereVsCapsule(const FVector& SphereCenter, float SphereRadius, const FCapsuleSegment& Capsule,
		FVector& OutNormal, float& OutDepth)
	{
		const FVector Closest = ClosestPointOnSegment(SphereCenter, Capsule.A, Capsule.B);
		return FCollisionMath::SphereVsSphere(SphereCenter, SphereRadius, Closest, Capsule.Radius, OutNormal, OutDepth);
	}

	bool CapsuleVsCapsule(const FCapsuleSegment& A, const FCapsuleSegment& B, FVector& OutNormal, float& OutDepth)
	{
		FVector CA;
		FVector CB;
		ClosestPointsOnSegments(A.A, A.B, B.A, B.B, CA, CB);
		return FCollisionMath::SphereVsSphere(CA, A.Radius, CB, B.Radius, OutNormal, OutDepth);
	}

	bool RaySphere(const FVector& Start, const FVector& Dir, float MaxDist, const FVector& Center, float Radius,
		float& OutDistance, FVector& OutNormal)
	{
		if (Radius < 0.0f)
		{
			return false;
		}

		const FVector LocalStart = Start - Center;
		const float B = LocalStart.Dot(Dir);
		const float C = LocalStart.Dot(LocalStart) - Radius * Radius;
		if (C <= 0.0f)
		{
			OutDistance = 0.0f;
			OutNormal = SafeNormal(LocalStart);
			return true;
		}

		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		const float T = -B - std::sqrt(Discriminant);
		if (T < 0.0f || T > MaxDist)
		{
			return false;
		}

		OutDistance = T;
		OutNormal = SafeNormal(Start + Dir * T - Center);
		return true;
	}

	bool RayExpandedOrientedBox(const FVector& Start, const FVector& Dir, float MaxDist, const FOrientedBox& Box,
		float Radius, float& OutDistance, FVector& OutNormal)
	{
		const FVector Ext = Box.Extent + FVector(Radius, Radius, Radius);
		const FVector LocalStart(
			(Start - Box.Center).Dot(Box.Axis[0]),
			(Start - Box.Center).Dot(Box.Axis[1]),
			(Start - Box.Center).Dot(Box.Axis[2]));
		const FVector LocalDir(Dir.Dot(Box.Axis[0]), Dir.Dot(Box.Axis[1]), Dir.Dot(Box.Axis[2]));

		float TEnter = 0.0f;
		float TExit = MaxDist;
		FVector EnterNormal = FVector::ZeroVector;

		auto TestAxis = [&](float StartValue, float DirValue, float Extent, const FVector& NegativeNormal, const FVector& PositiveNormal) -> bool
		{
			if (std::abs(DirValue) <= Epsilon)
			{
				return StartValue >= -Extent && StartValue <= Extent;
			}

			float T1 = (-Extent - StartValue) / DirValue;
			float T2 = (Extent - StartValue) / DirValue;
			FVector AxisNormal = NegativeNormal;
			if (T1 > T2)
			{
				std::swap(T1, T2);
				AxisNormal = PositiveNormal;
			}

			if (T1 > TEnter)
			{
				TEnter = T1;
				EnterNormal = AxisNormal;
			}
			if (T2 < TExit)
			{
				TExit = T2;
			}

			return TEnter <= TExit;
		};

		if (!TestAxis(LocalStart.X, LocalDir.X, Ext.X, Box.Axis[0] * -1.0f, Box.Axis[0])) return false;
		if (!TestAxis(LocalStart.Y, LocalDir.Y, Ext.Y, Box.Axis[1] * -1.0f, Box.Axis[1])) return false;
		if (!TestAxis(LocalStart.Z, LocalDir.Z, Ext.Z, Box.Axis[2] * -1.0f, Box.Axis[2])) return false;

		if (TExit < 0.0f || TEnter > MaxDist)
		{
			return false;
		}

		OutDistance = TEnter > 0.0f ? TEnter : 0.0f;
		OutNormal = SafeNormal(EnterNormal, Start + Dir * OutDistance - Box.Center);
		return true;
	}

	FOrientedBox MakeCurvedWallPanelBox(const UCurvedWallRunColliderComponent* Curve, int32 SegmentIndex)
	{
		const int32 SegmentCount = Curve->GetSafeSegmentCount();
		const float ArcRadians = Curve->GetArcAngleRadians();
		const float SegmentAngle = ArcRadians / static_cast<float>(SegmentCount);
		const float Alpha = (static_cast<float>(SegmentIndex) + 0.5f) / static_cast<float>(SegmentCount);
		const float Angle = -ArcRadians * 0.5f + ArcRadians * Alpha;
		const float CosAngle = std::cos(Angle);
		const float SinAngle = std::sin(Angle);

		const FVector AxisX = SafeNormal(Curve->GetForwardVector(), FVector::XAxisVector);
		const FVector AxisY = SafeNormal(Curve->GetRightVector(), FVector::YAxisVector);
		const FVector AxisZ = SafeNormal(Curve->GetUpVector(), FVector::ZAxisVector);
		const FVector Radial = SafeNormal(AxisX * CosAngle + AxisY * SinAngle, AxisX);
		const FVector Tangent = SafeNormal(AxisX * -SinAngle + AxisY * CosAngle, AxisY);
		const float Radius = Curve->GetScaledRadius();
		const float HalfLength = (std::max)(0.05f, Radius * std::tan(SegmentAngle * 0.5f) * 1.05f);

		FOrientedBox Result;
		Result.Center = Curve->GetWorldLocation() + Radial * Radius;
		Result.Axis[0] = Tangent;
		Result.Axis[1] = Radial;
		Result.Axis[2] = AxisZ;
		Result.Extent = FVector(HalfLength, Curve->GetScaledThickness() * 0.5f, Curve->GetScaledHalfHeight());
		return Result;
	}

	FVector MakeCurvedWallNormal(const UCurvedWallRunColliderComponent* Curve, const FVector& HitLocation, const FVector& Dir)
	{
		const FVector AxisX = SafeNormal(Curve->GetForwardVector(), FVector::XAxisVector);
		const FVector AxisY = SafeNormal(Curve->GetRightVector(), FVector::YAxisVector);
		const FVector FromCenter = HitLocation - Curve->GetWorldLocation();
		FVector Normal = AxisX * FromCenter.Dot(AxisX) + AxisY * FromCenter.Dot(AxisY);
		Normal = SafeNormal(Normal, AxisX);

		// sweep 방향과 같은 쪽을 보는 normal이면 반전해서 항상 접근 방향을 마주보게 한다.
		if (Normal.Dot(Dir) > 0.0f)
		{
			Normal *= -1.0f;
		}

		return Normal;
	}

	bool RayCurvedWall(const FVector& Start, const FVector& Dir, float MaxDist, float Radius,
		const UCurvedWallRunColliderComponent* Curve, float& OutDistance, FVector& OutNormal)
	{
		if (!Curve) return false;

		bool bFound = false;
		float BestDistance = MaxDist;
		FVector BestNormal = FVector::ZeroVector;

		const int32 SegmentCount = Curve->GetSafeSegmentCount();
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			float CandidateDistance = MaxDist;
			FVector CandidateNormal = FVector::ZeroVector;
			const FOrientedBox PanelBox = MakeCurvedWallPanelBox(Curve, SegmentIndex);
			if (!RayExpandedOrientedBox(Start, Dir, MaxDist, PanelBox, Radius, CandidateDistance, CandidateNormal))
			{
				continue;
			}
			if (CandidateDistance >= BestDistance)
			{
				continue;
			}

			const FVector HitLocation = Start + Dir * CandidateDistance;
			BestDistance = CandidateDistance;
			BestNormal = MakeCurvedWallNormal(Curve, HitLocation, Dir);
			bFound = true;
		}

		if (!bFound)
		{
			return false;
		}

		OutDistance = BestDistance;
		OutNormal = BestNormal;
		return true;
	}

	bool RayVerticalCapsuleLocal(const FVector& LocalStart, const FVector& LocalDir, float MaxDist,
		float SegmentHalfLength, float Radius, float& OutDistance, FVector& OutLocalNormal)
	{
		bool bFound = false;
		float Closest = MaxDist;
		FVector BestNormal = FVector::UpVector;

		const float A = LocalDir.X * LocalDir.X + LocalDir.Y * LocalDir.Y;
		const float B = LocalStart.X * LocalDir.X + LocalStart.Y * LocalDir.Y;
		const float C = LocalStart.X * LocalStart.X + LocalStart.Y * LocalStart.Y - Radius * Radius;
		if (C <= 0.0f && LocalStart.Z >= -SegmentHalfLength && LocalStart.Z <= SegmentHalfLength)
		{
			OutDistance = 0.0f;
			OutLocalNormal = SafeNormal(FVector(LocalStart.X, LocalStart.Y, 0.0f));
			return true;
		}

		if (A > Epsilon)
		{
			const float Disc = B * B - A * C;
			if (Disc >= 0.0f)
			{
				const float SqrtDisc = std::sqrt(Disc);
				const float InvA = 1.0f / A;
				const float Candidates[2] = { (-B - SqrtDisc) * InvA, (-B + SqrtDisc) * InvA };
				for (float T : Candidates)
				{
					const float Z = LocalStart.Z + LocalDir.Z * T;
					if (T >= 0.0f && T <= Closest && Z >= -SegmentHalfLength && Z <= SegmentHalfLength)
					{
						Closest = T;
						BestNormal = SafeNormal(FVector(LocalStart.X + LocalDir.X * T, LocalStart.Y + LocalDir.Y * T, 0.0f));
						bFound = true;
					}
				}
			}
		}

		auto TestCap = [&](const FVector& Center)
		{
			float T = MaxDist;
			FVector Normal = FVector::ZeroVector;
			if (RaySphere(LocalStart, LocalDir, Closest, Center, Radius, T, Normal) && T <= Closest)
			{
				Closest = T;
				BestNormal = Normal;
				bFound = true;
			}
		};

		TestCap(FVector(0.0f, 0.0f, SegmentHalfLength));
		TestCap(FVector(0.0f, 0.0f, -SegmentHalfLength));

		if (!bFound)
		{
			return false;
		}

		OutDistance = Closest;
		OutLocalNormal = BestNormal;
		return true;
	}
}

bool FCollisionMath::AABBvsAABB(
	const FVector& MinA, const FVector& MaxA,
	const FVector& MinB, const FVector& MaxB)
{
	return (MinA.X <= MaxB.X && MaxA.X >= MinB.X)
		&& (MinA.Y <= MaxB.Y && MaxA.Y >= MinB.Y)
		&& (MinA.Z <= MaxB.Z && MaxA.Z >= MinB.Z);
}

bool FCollisionMath::SphereVsSphere(
	const FVector& CenterA, float RadiusA,
	const FVector& CenterB, float RadiusB,
	FVector& OutNormal, float& OutDepth)
{
	const FVector Delta = CenterB - CenterA;
	const float DistSq = Delta.Dot(Delta);
	const float SumR = RadiusA + RadiusB;

	if (DistSq > SumR * SumR)
	{
		return false;
	}

	const float Dist = std::sqrt((std::max)(0.0f, DistSq));
	if (Dist > Epsilon)
	{
		OutNormal = Delta * (1.0f / Dist);
	}
	else
	{
		OutNormal = FVector::UpVector;
	}
	OutDepth = SumR - Dist;
	return true;
}

bool FCollisionMath::BoxVsSphere(
	const FVector& BoxMin, const FVector& BoxMax,
	const FVector& SphereCenter, float SphereRadius,
	FVector& OutNormal, float& OutDepth)
{
	FVector Closest;
	Closest.X = ClampFloat(SphereCenter.X, BoxMin.X, BoxMax.X);
	Closest.Y = ClampFloat(SphereCenter.Y, BoxMin.Y, BoxMax.Y);
	Closest.Z = ClampFloat(SphereCenter.Z, BoxMin.Z, BoxMax.Z);

	const FVector Delta = SphereCenter - Closest;
	const float DistSq = Delta.Dot(Delta);

	if (DistSq > SphereRadius * SphereRadius)
	{
		return false;
	}

	const float Dist = std::sqrt((std::max)(0.0f, DistSq));
	if (Dist > Epsilon)
	{
		OutNormal = Delta * (1.0f / Dist);
		OutDepth = SphereRadius - Dist;
	}
	else
	{
		const FVector BoxCenter = (BoxMin + BoxMax) * 0.5f;
		const FVector HalfExt = (BoxMax - BoxMin) * 0.5f;
		const FVector LocalPos = SphereCenter - BoxCenter;

		float MinPen = FLT_MAX;
		int32 MinAxis = 0;
		float MinSign = 1.0f;

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Pen = HalfExt.Data[AxisIndex] - std::abs(LocalPos.Data[AxisIndex]) + SphereRadius;
			if (Pen < MinPen)
			{
				MinPen = Pen;
				MinAxis = AxisIndex;
				MinSign = (LocalPos.Data[AxisIndex] >= 0.0f) ? 1.0f : -1.0f;
			}
		}

		OutNormal = FVector::ZeroVector;
		OutNormal.Data[MinAxis] = MinSign;
		OutDepth = MinPen;
	}
	return true;
}

bool FCollisionMath::SweepSphereShapeComponent(
	const FVector& Start,
	const FVector& Dir,
	float MaxDist,
	float Radius,
	UPrimitiveComponent* Shape,
	FHitResult& OutHit)
{
	if (!Shape || MaxDist <= 0.0f || Radius < 0.0f)
	{
		return false;
	}

	float HitDistance = MaxDist;
	FVector HitNormal = FVector::ZeroVector;
	bool bHit = false;

	if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
	{
		bHit = RaySphere(Start, Dir, MaxDist, Sphere->GetWorldLocation(),
			Sphere->GetScaledSphereRadius() + Radius, HitDistance, HitNormal);
	}
	else if (UBoxComponent* Box = Cast<UBoxComponent>(Shape))
	{
		bHit = RayExpandedOrientedBox(Start, Dir, MaxDist, MakeOrientedBox(Box),
			Radius, HitDistance, HitNormal);
	}
	else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape))
	{
		const FVector AxisX = SafeNormal(Capsule->GetForwardVector(), FVector::XAxisVector);
		const FVector AxisY = SafeNormal(Capsule->GetRightVector(), FVector::YAxisVector);
		const FVector AxisZ = SafeNormal(Capsule->GetUpVector(), FVector::ZAxisVector);
		const FVector RelStart = Start - Capsule->GetWorldLocation();
		const FVector LocalStart(RelStart.Dot(AxisX), RelStart.Dot(AxisY), RelStart.Dot(AxisZ));
		const FVector LocalDir(Dir.Dot(AxisX), Dir.Dot(AxisY), Dir.Dot(AxisZ));
		const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
		const float SegmentHalfLength = (std::max)(0.0f, Capsule->GetScaledCapsuleHalfHeight() - CapsuleRadius);

		FVector LocalNormal = FVector::ZeroVector;
		bHit = RayVerticalCapsuleLocal(LocalStart, LocalDir, MaxDist,
			SegmentHalfLength, CapsuleRadius + Radius, HitDistance, LocalNormal);
		if (bHit)
		{
			HitNormal = SafeNormal(AxisX * LocalNormal.X + AxisY * LocalNormal.Y + AxisZ * LocalNormal.Z);
		}
	}
	else if (UCurvedWallRunColliderComponent* Curve = Cast<UCurvedWallRunColliderComponent>(Shape))
	{
		bHit = RayCurvedWall(Start, Dir, MaxDist, Radius, Curve, HitDistance, HitNormal);
	}

	if (!bHit)
	{
		return false;
	}

	OutHit.bHit = true;
	OutHit.Distance = HitDistance;
	OutHit.WorldHitLocation = Start + Dir * HitDistance;
	OutHit.ImpactNormal = HitNormal;
	OutHit.WorldNormal = HitNormal;
	OutHit.HitComponent = Shape;
	OutHit.HitActor = Shape->GetOwner();
	return true;
}

EShapeType FCollisionMath::GetShapeType(const UPrimitiveComponent* Comp)
{
	if (Comp->IsA<UBoxComponent>()) return EShapeType::Box;
	if (Comp->IsA<USphereComponent>()) return EShapeType::Sphere;
	if (Comp->IsA<UCapsuleComponent>()) return EShapeType::Capsule;
	return EShapeType::AABB;
}

bool FCollisionMath::TestComponentPair(
	UPrimitiveComponent* A,
	UPrimitiveComponent* B,
	FHitResult& OutHit)
{
	EShapeType TypeA = GetShapeType(A);
	EShapeType TypeB = GetShapeType(B);

	if (TypeA > TypeB)
	{
		std::swap(A, B);
		std::swap(TypeA, TypeB);
	}

	FVector Normal = FVector::UpVector;
	float Depth = 0.0f;
	bool bHit = false;

	if (TypeA == EShapeType::AABB || TypeB == EShapeType::AABB)
	{
		const FBoundingBox BoundsA = A->GetWorldBoundingBox();
		const FBoundingBox BoundsB = B->GetWorldBoundingBox();
		bHit = AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max);
		if (bHit)
		{
			Normal = SafeNormal(BoundsB.GetCenter() - BoundsA.GetCenter());
		}
	}
	else if (TypeA == EShapeType::Box && TypeB == EShapeType::Box)
	{
		bHit = OBBvsOBB(MakeOrientedBox(static_cast<UBoxComponent*>(A)),
			MakeOrientedBox(static_cast<UBoxComponent*>(B)), Normal, Depth);
	}
	else if (TypeA == EShapeType::Box && TypeB == EShapeType::Sphere)
	{
		bHit = OBBvsSphere(MakeOrientedBox(static_cast<UBoxComponent*>(A)),
			B->GetWorldLocation(), static_cast<USphereComponent*>(B)->GetScaledSphereRadius(),
			Normal, Depth);
	}
	else if (TypeA == EShapeType::Box && TypeB == EShapeType::Capsule)
	{
		bHit = OBBvsCapsule(MakeOrientedBox(static_cast<UBoxComponent*>(A)),
			MakeCapsuleSegment(static_cast<UCapsuleComponent*>(B)), Normal, Depth);
	}
	else if (TypeA == EShapeType::Sphere && TypeB == EShapeType::Sphere)
	{
		bHit = SphereVsSphere(A->GetWorldLocation(), static_cast<USphereComponent*>(A)->GetScaledSphereRadius(),
			B->GetWorldLocation(), static_cast<USphereComponent*>(B)->GetScaledSphereRadius(),
			Normal, Depth);
	}
	else if (TypeA == EShapeType::Sphere && TypeB == EShapeType::Capsule)
	{
		bHit = SphereVsCapsule(A->GetWorldLocation(), static_cast<USphereComponent*>(A)->GetScaledSphereRadius(),
			MakeCapsuleSegment(static_cast<UCapsuleComponent*>(B)), Normal, Depth);
	}
	else if (TypeA == EShapeType::Capsule && TypeB == EShapeType::Capsule)
	{
		bHit = CapsuleVsCapsule(MakeCapsuleSegment(static_cast<UCapsuleComponent*>(A)),
			MakeCapsuleSegment(static_cast<UCapsuleComponent*>(B)), Normal, Depth);
	}

	if (bHit)
	{
		OutHit.bHit = true;
		OutHit.ImpactNormal = Normal;
		OutHit.WorldNormal = Normal;
		OutHit.PenetrationDepth = Depth;
		OutHit.WorldHitLocation = A->GetWorldLocation() + Normal * 0.5f;
		OutHit.HitComponent = B;
		OutHit.HitActor = B->GetOwner();
	}

	return bHit;
}
