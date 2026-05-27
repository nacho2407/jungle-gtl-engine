#pragma once
#include "PrimitiveComponent.h"

UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UShapeComponent, UPrimitiveComponent)

	void PostDuplicate(UObject* Original) override;

	/**
	 * @brief 움직이는 점 segment와 이 shape의 최초 접촉 결과를 계산한다.
	 * @param OutHit 충돌 시 접촉점, normal, 이동 구간 안의 hit 시간을 채울 결과값
	 * @param Params 시작 겹침 검사 여부 같은 query 조건
	 * @note 기본 shape는 판정하지 않고, 실제 Sphere/Capsule/Box component가 구현
	 */
	virtual bool LineTraceShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionQueryParams& Params) const;

	/**
	 * @brief 움직이는 sphere와 이 shape의 최초 접촉 결과를 계산한다.
	 * @note line에 가까운 형상은 파생 component에서 line trace와 같은 경로로 처리할 수 있다.
	 */
	virtual bool SweepShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params) const;

private:
	UPROPERTY(DisplayName = "Shape Color")
	FColor ShapeColor;

	UPROPERTY(DisplayName = "Draw Only If Selected")
	bool bDrawOnlyIfSelected;

	// UPrimitiveComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
