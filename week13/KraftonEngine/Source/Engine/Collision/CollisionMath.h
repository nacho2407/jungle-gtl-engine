#pragma once

#include "Math/Vector.h"
#include "Core/CollisionTypes.h"

class UPrimitiveComponent;

// ============================================================
// EShapeType — 내로우페이즈 디스패치용 셰이프 열거
// ============================================================
enum class EShapeType : uint8
{
	AABB,		// 일반 PrimitiveComponent (메시 등) — AABB 폴백
	Box,
	Sphere,
	Capsule
};

// ============================================================
// FCollisionMath — 셰이프 간 교차 판정 함수 모음
// ============================================================
namespace FCollisionMath
{
	// --- AABB vs AABB ---
	bool AABBvsAABB(
		const FVector& MinA, const FVector& MaxA,
		const FVector& MinB, const FVector& MaxB);

	// --- Sphere vs Sphere ---
	// 관통 시 OutNormal = A→B 방향 단위벡터, OutDepth = 관통 깊이
	bool SphereVsSphere(
		const FVector& CenterA, float RadiusA,
		const FVector& CenterB, float RadiusB,
		FVector& OutNormal, float& OutDepth);

	// --- Box(AABB) vs Sphere ---
	// Box는 월드 축 정렬 AABB로 취급
	bool BoxVsSphere(
		const FVector& BoxMin, const FVector& BoxMax,
		const FVector& SphereCenter, float SphereRadius,
		FVector& OutNormal, float& OutDepth);

	// --- 유틸리티: 컴포넌트 쌍의 셰이프 타입 판별 ---
	EShapeType GetShapeType(const UPrimitiveComponent* Comp);

	// --- 최상위 디스패치: 두 컴포넌트 간 내로우페이즈 ---
	// 교차 시 OutHit에 관통 방향(Normal)과 깊이를 채움
	bool TestComponentPair(
		UPrimitiveComponent* A,
		UPrimitiveComponent* B,
		FHitResult& OutHit);
}
