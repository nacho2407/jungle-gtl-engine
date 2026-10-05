#pragma once
#include "ShapeComponent.h"

UCLASS(SpawnableComponent, DisplayName = "Sphere Component", Category = "Collision")
class USphereComponent : public UShapeComponent
{
public:
	GENERATED_BODY(USphereComponent, UShapeComponent)
	float GetSphereRadius() const { return SphereRadius; }

	/**
	 * @brief world scale을 반영한 sphere 충돌 반지름을 반환한다.
	 * @note 비균일 scale에서는 가장 큰 절대 축을 써서 bounds와 query가 같은 구를 보게 한다.
	 */
	float GetScaledSphereRadius() const;

	void PostDuplicate(UObject* Original) override;

	/**
	 * @brief 움직이는 점 segment와 sphere target의 최초 접촉 결과를 계산한다.
	 */
	bool LineTraceShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionQueryParams& Params) const override;

	/**
	 * @brief 움직이는 sphere와 sphere target의 최초 접촉 결과를 계산한다.
	 * @param Params 시작부터 겹친 경우를 보고할지 정하는 조건이다.
	 */
	bool SweepShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params) const override;

private:
	UPROPERTY(DisplayName = "Sphere Radius", LuaReadOnly, LuaName = SphereRadius)
	float SphereRadius = 0.5f;

	/**
	 * @brief target 중심에서 query 중심으로 향하는 hit normal을 만든다.
	 * @param CenterToQuery target 중심에서 query 중심까지의 벡터
	 * @param Move query가 이번 검사에서 이동한 벡터
	 * @note 중심이 완전히 겹쳐 방향을 정할 수 없으면 이동 반대 방향 또는 위쪽 방향을 사용한다.
	 */
	static FVector MakeHitNormal(const FVector& CenterToQuery, const FVector& Move);

	/**
	 * @brief segment가 sphere 표면에 처음 닿는 이동 시각을 계산한다.
	 * @param OutTime 최초 접촉 이동 비율 `0~1`을 받는다.
	 * @param OutNormal target 바깥 방향 normal을 받는다.
	 * @param bOutStartPenetrating 시작부터 겹쳐 있었는지 받는다.
	 * @note 두 교점 중 먼저 만나는 해만 반환한다.
	 */
	static bool FindSegmentSphereFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& TargetCenterWS,
		float ExpandedRadius,
		bool bFindInitialOverlaps,
		float& OutTime,
		FVector& OutNormal,
		bool& bOutStartPenetrating);

	/**
	 * @brief 계산한 sphere hit 정보를 공통 `FHitResult` 계약에 맞게 기록한다.
	 * @param Time 최초 접촉 이동 비율
	 * @param Normal target 바깥 방향 normal
	 * @param bStartPenetrating 시작부터 겹친 hit인지 여부다.
	 * @note 접촉점은 확장된 구가 아니라 원래 target sphere 표면에 기록한다.
	 */
	void FillHitResult(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& TargetCenterWS,
		float TargetRadius,
		float Time,
		const FVector& Normal,
		bool bStartPenetrating) const;

	// UShapeComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
