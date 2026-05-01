#include "Collision/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"

#include <cmath>
#include <algorithm>

// ============================================================
// AABB vs AABB
// ============================================================
bool FCollisionMath::AABBvsAABB(
	const FVector& MinA, const FVector& MaxA,
	const FVector& MinB, const FVector& MaxB)
{
	return (MinA.X <= MaxB.X && MaxA.X >= MinB.X)
		&& (MinA.Y <= MaxB.Y && MaxA.Y >= MinB.Y)
		&& (MinA.Z <= MaxB.Z && MaxA.Z >= MinB.Z);
}

// ============================================================
// Sphere vs Sphere
// ============================================================
bool FCollisionMath::SphereVsSphere(
	const FVector& CenterA, float RadiusA,
	const FVector& CenterB, float RadiusB,
	FVector& OutNormal, float& OutDepth)
{
	FVector Delta = CenterB - CenterA;
	float DistSq = Delta.Dot(Delta);
	float SumR = RadiusA + RadiusB;

	if (DistSq >= SumR * SumR)
	{
		return false;
	}

	float Dist = std::sqrt(DistSq);
	if (Dist > 1e-6f)
	{
		OutNormal = Delta * (1.0f / Dist);
	}
	else
	{
		OutNormal = FVector(0.0f, 0.0f, 1.0f);
	}
	OutDepth = SumR - Dist;
	return true;
}

// ============================================================
// Box(AABB) vs Sphere — closest point 방식
// ============================================================
bool FCollisionMath::BoxVsSphere(
	const FVector& BoxMin, const FVector& BoxMax,
	const FVector& SphereCenter, float SphereRadius,
	FVector& OutNormal, float& OutDepth)
{
	// Box 위의 가장 가까운 점
	FVector Closest;
	Closest.X = (std::max)(BoxMin.X, (std::min)(SphereCenter.X, BoxMax.X));
	Closest.Y = (std::max)(BoxMin.Y, (std::min)(SphereCenter.Y, BoxMax.Y));
	Closest.Z = (std::max)(BoxMin.Z, (std::min)(SphereCenter.Z, BoxMax.Z));

	FVector Delta = SphereCenter - Closest;
	float DistSq = Delta.Dot(Delta);

	if (DistSq >= SphereRadius * SphereRadius)
	{
		return false;
	}

	float Dist = std::sqrt(DistSq);
	if (Dist > 1e-6f)
	{
		OutNormal = Delta * (1.0f / Dist);
		OutDepth = SphereRadius - Dist;
	}
	else
	{
		// Sphere 중심이 Box 내부에 있는 경우 — 최소 관통 축을 찾음
		FVector BoxCenter = (BoxMin + BoxMax) * 0.5f;
		FVector HalfExt = (BoxMax - BoxMin) * 0.5f;
		FVector LocalPos = SphereCenter - BoxCenter;

		float MinPen = FLT_MAX;
		int MinAxis = 0;
		float MinSign = 1.0f;

		for (int i = 0; i < 3; ++i)
		{
			float Pen = HalfExt.Data[i] - std::abs(LocalPos.Data[i]) + SphereRadius;
			if (Pen < MinPen)
			{
				MinPen = Pen;
				MinAxis = i;
				MinSign = (LocalPos.Data[i] >= 0.0f) ? 1.0f : -1.0f;
			}
		}

		OutNormal = FVector(0, 0, 0);
		OutNormal.Data[MinAxis] = MinSign;
		OutDepth = MinPen;
	}
	return true;
}

// ============================================================
// 셰이프 타입 판별
// ============================================================
EShapeType FCollisionMath::GetShapeType(const UPrimitiveComponent* Comp)
{
	if (Comp->IsA<UBoxComponent>()) return EShapeType::Box;
	if (Comp->IsA<USphereComponent>()) return EShapeType::Sphere;
	if (Comp->IsA<UCapsuleComponent>()) return EShapeType::Capsule;
	return EShapeType::AABB;
}

// ============================================================
// 두 컴포넌트 간 내로우페이즈 디스패치
// ============================================================
bool FCollisionMath::TestComponentPair(
	UPrimitiveComponent* A,
	UPrimitiveComponent* B,
	FHitResult& OutHit)
{
	EShapeType TypeA = GetShapeType(A);
	EShapeType TypeB = GetShapeType(B);

	// 순서 정규화: 작은 타입이 항상 A
	if (TypeA > TypeB)
	{
		std::swap(A, B);
		std::swap(TypeA, TypeB);
	}

	FVector Normal;
	float Depth;
	bool bHit = false;

	// --- AABB vs anything → AABB 폴백 ---
	if (TypeA == EShapeType::AABB || TypeB == EShapeType::AABB)
	{
		FBoundingBox BoundsA = A->GetWorldBoundingBox();
		FBoundingBox BoundsB = B->GetWorldBoundingBox();
		bHit = AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max);
		if (bHit)
		{
			// 간이 관통 방향: A→B 중심 방향
			FVector CenterA = BoundsA.GetCenter();
			FVector CenterB = BoundsB.GetCenter();
			Normal = CenterB - CenterA;
			float Len = std::sqrt(Normal.Dot(Normal));
			if (Len > 1e-6f) Normal = Normal * (1.0f / Len);
			else Normal = FVector(0, 0, 1);
			Depth = 0.0f; // AABB에서는 정확한 관통 깊이 미산출
		}
	}
	// --- Box vs Box → AABB ---
	else if (TypeA == EShapeType::Box && TypeB == EShapeType::Box)
	{
		FBoundingBox BoundsA = A->GetWorldBoundingBox();
		FBoundingBox BoundsB = B->GetWorldBoundingBox();
		bHit = AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max);
		if (bHit)
		{
			// 최소 관통 축 계산
			FVector CenterA = BoundsA.GetCenter();
			FVector CenterB = BoundsB.GetCenter();
			FVector HalfA = BoundsA.GetExtent();
			FVector HalfB = BoundsB.GetExtent();
			FVector Delta = CenterB - CenterA;

			float MinPen = FLT_MAX;
			int MinAxis = 0;
			for (int i = 0; i < 3; ++i)
			{
				float Overlap = (HalfA.Data[i] + HalfB.Data[i]) - std::abs(Delta.Data[i]);
				if (Overlap < MinPen)
				{
					MinPen = Overlap;
					MinAxis = i;
				}
			}
			Normal = FVector(0, 0, 0);
			Normal.Data[MinAxis] = (Delta.Data[MinAxis] >= 0.0f) ? 1.0f : -1.0f;
			Depth = MinPen;
		}
	}
	// --- Box vs Sphere ---
	else if (TypeA == EShapeType::Box && TypeB == EShapeType::Sphere)
	{
		FBoundingBox BoxBounds = A->GetWorldBoundingBox();
		const USphereComponent* Sphere = static_cast<const USphereComponent*>(B);
		bHit = BoxVsSphere(
			BoxBounds.Min, BoxBounds.Max,
			B->GetWorldLocation(), Sphere->GetScaledSphereRadius(),
			Normal, Depth);
	}
	// --- Sphere vs Sphere ---
	else if (TypeA == EShapeType::Sphere && TypeB == EShapeType::Sphere)
	{
		const USphereComponent* SA = static_cast<const USphereComponent*>(A);
		const USphereComponent* SB = static_cast<const USphereComponent*>(B);
		bHit = SphereVsSphere(
			A->GetWorldLocation(), SA->GetScaledSphereRadius(),
			B->GetWorldLocation(), SB->GetScaledSphereRadius(),
			Normal, Depth);
	}
	// --- Capsule 조합 등 미구현 → AABB 폴백 ---
	else
	{
		FBoundingBox BoundsA = A->GetWorldBoundingBox();
		FBoundingBox BoundsB = B->GetWorldBoundingBox();
		bHit = AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max);
		if (bHit)
		{
			FVector CenterA = BoundsA.GetCenter();
			FVector CenterB = BoundsB.GetCenter();
			Normal = CenterB - CenterA;
			float Len = std::sqrt(Normal.Dot(Normal));
			if (Len > 1e-6f) Normal = Normal * (1.0f / Len);
			else Normal = FVector(0, 0, 1);
			Depth = 0.0f;
		}
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
