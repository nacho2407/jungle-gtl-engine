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

	/**
	 * @brief 두께가 없는 line query 형상을 만든다.
	 * @note radius가 0인 particle query는 line trace 경로로 보낸다.
	 */
	static FCollisionShape MakeLine()
	{
		return FCollisionShape{};
	}

	/**
	 * @brief 반지름을 가진 sphere sweep query 형상을 만든다.
	 * @param Radius 움직이는 query sphere의 반지름이다.
	 * @note 음수 반지름은 0으로 고쳐 line query와 같은 의미가 되게 한다.
	 */
	static FCollisionShape MakeSphere(float Radius)
	{
		FCollisionShape Shape;
		Shape.ShapeType = ECollisionShapeType::Sphere;
		Shape.SphereRadius = Radius > 0.0f ? Radius : 0.0f;
		return Shape;
	}

	/**
	 * @brief 이 query가 두께 없는 line으로 처리되어야 하는지 확인한다.
	 * @note 매우 작은 sphere radius는 별도 sweep을 만들지 않고 line으로 처리한다.
	 */
	bool IsNearlyZero() const
	{
		return ShapeType == ECollisionShapeType::Line || SphereRadius <= 1.e-6f;
	}

	/**
	 * @brief 실제 반지름을 가진 sphere sweep query인지 확인한다.
	 * @note `IsNearlyZero()`인 형상은 sphere type이어도 sweep 대상으로 보지 않는다.
	 */
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

	/**
	 * @brief 이전 query 결과를 지우고 hit이 없는 초기 상태로 되돌린다.
	 * @note 새 후보를 검사하기 전에 호출해서 이전 component 정보가 남지 않게 한다.
	 */
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

	/**
	 * @brief 실제 component까지 확정된 hit 결과인지 확인한다.
	 * @note broad phase 후보만으로는 valid hit이 아니며 narrow phase 결과만 true가 된다.
	 */
	bool IsValid() const
	{
		return bHit && (HitComponent != nullptr);
	}
};
