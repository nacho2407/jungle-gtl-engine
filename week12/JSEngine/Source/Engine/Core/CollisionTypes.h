#pragma once
#include "Math/Vector.h" // 필요한 최소한의 수학 라이브러리만

class AActor;
class UPrimitiveComponent;

enum class ECollisionShapeType
{
	Line,
	Sphere,
};

struct FCollisionShape
{
	ECollisionShapeType ShapeType = ECollisionShapeType::Line;
	float SphereRadius = 0.0f;

	static FCollisionShape MakeLine()
	{
		return FCollisionShape{};
	}

	static FCollisionShape MakeSphere(float Radius)
	{
		FCollisionShape Shape;
		Shape.ShapeType = ECollisionShapeType::Sphere;
		Shape.SphereRadius = Radius > 0.0f ? Radius : 0.0f;
		return Shape;
	}

	bool IsNearlyZero() const
	{
		return ShapeType == ECollisionShapeType::Line || SphereRadius <= 1.e-6f;
	}

	bool IsSphere() const
	{
		return ShapeType == ECollisionShapeType::Sphere && !IsNearlyZero();
	}
};

struct FCollisionQueryParams
{
	const AActor* IgnoredActor = nullptr;
	const UPrimitiveComponent* IgnoredComponent = nullptr;
	bool bFindInitialOverlaps = true;
};

struct FHitResult
{
	UPrimitiveComponent* HitComponent = nullptr;

	float Distance = FLT_MAX;
	float Time = 1.0f;

	// Location은 target 표면 접촉점이다.
	// particle 중심으로 쓰면 위치 보정이 틀어진다.
	FVector Location = { 0, 0, 0 };
	FVector Normal = { 0, 0, 0 };

	int FaceIndex = -1;

	bool bHit = false;
	bool bStartPenetrating = false;

	void Reset()
	{
		HitComponent = nullptr;
		Distance = FLT_MAX;
		Time = 1.0f;
		Location = { 0, 0, 0 };
		Normal = { 0, 0, 0 };
		FaceIndex = -1;
		bHit = false;
		bStartPenetrating = false;
	}

	bool IsValid() const
	{
		return bHit && (HitComponent != nullptr);
	}
};
