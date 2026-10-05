#pragma once
#include "ShapeComponent.h"

UCLASS(SpawnableComponent, DisplayName = "Capsule Component", Category = "Collision")
class UCapsuleComponent : public UShapeComponent
{
public:
	GENERATED_BODY(UCapsuleComponent, UShapeComponent)
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	float GetCapsuleRadius() const { return CapsuleRadius; }

	void UpdateWorldAABB() const override;

	/**
	 * @brief local Z 방향 길이에 world scale을 반영한 capsule 반높이를 반환한다.
	 * @note 캡슐의 축은 기존 정의대로 local Z를 유지한다.
	 */
	float GetScaledCapsuleHalfHeight() const;

	/**
	 * @brief local X/Y 방향 굵기에 world scale을 반영한 capsule 반지름을 반환한다.
	 * @note X/Y 중 더 큰 절대 scale을 써서 bounds와 narrow phase가 같은 굵기를 본다.
	 */
	float GetScaledCapsuleRadius() const;

	/**
	 * @brief 움직이는 점 segment와 capsule target의 최초 접촉 결과를 계산한다.
	 * @note 옆면과 양 끝 구 후보 중 가장 빠른 접촉만 결과로 반환한다.
	 */
	bool LineTraceShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionQueryParams& Params) const override;

	/**
	 * @brief 움직이는 sphere와 capsule target의 최초 접촉 결과를 계산한다.
	 * @note hit 시간은 반지름을 늘린 capsule로 계산하지만, Location은 원래 target 표면에 둔다.
	 */
	bool SweepShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params) const override;

private:
	UPROPERTY(DisplayName = "Capsule Half Height", LuaReadOnly, LuaName = CapsuleHalfHeight)
	float CapsuleHalfHeight = 0.5f;

	UPROPERTY(DisplayName = "Capsule Radius", LuaReadOnly, LuaName = CapsuleRadius)
	float CapsuleRadius = 0.5f;

	/**
	 * @brief 한 점에서 capsule 중심 축 segment로 가장 가까운 점을 반환한다.
	 * @note 접촉점과 normal은 무한 직선이 아니라 양 끝이 있는 capsule 축 기준으로 계산
	 */
	static FVector ClosestPointOnAxis(
		const FVector& Point,
		const FVector& Center,
		const FVector& Axis,
		float HalfHeight);

	/**
	 * @brief capsule 축에서 query 중심으로 향하는 hit normal을 만든다.
	 * @note query가 중심선 위에 있어 방향이 정해지지 않으면 축에 수직인 안정적인 방향을 고른다.
	 */
	static FVector MakeHitNormal(
		const FVector& AxisPointToQuery,
		const FVector& Move,
		const FVector& Axis);

	/**
	 * @brief capsule 양 끝 구 하나에 대해 segment의 최초 접촉 시각을 계산한다.
	 * @note capsule 전체 판정 중 cap 후보를 계산하는 보조 함수다.
	 */
	static bool FindSegmentSphereFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& SphereCenterWS,
		float Radius,
		float& OutTime);

	/**
	 * @brief segment가 capsule에 처음 닿는 이동 시각과 normal을 계산한다.
	 * @note 최근접 위치는 최초 접촉 시각이 아니므로 옆면과 양 끝 구의 후보를 따로 비교함
	 */
	static bool FindSegmentCapsuleFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& CapsuleCenterWS,
		const FVector& CapsuleAxis,
		float HalfHeight,
		float ExpandedRadius,
		bool bFindInitialOverlaps,
		float& OutTime,
		FVector& OutNormal,
		bool& bOutStartPenetrating);

	/**
	 * @brief 계산한 capsule hit 정보를 공통 `FHitResult` 계약에 맞게 기록한다.
	 * @note 접촉점은 확장된 capsule이 아니라 원래 target capsule 표면에 기록한다.
	 */
	void FillHitResult(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& CapsuleCenterWS,
		const FVector& CapsuleAxis,
		float CapsuleHalfHeight,
		float TargetRadius,
		float Time,
		const FVector& Normal,
		bool bStartPenetrating) const;

	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
