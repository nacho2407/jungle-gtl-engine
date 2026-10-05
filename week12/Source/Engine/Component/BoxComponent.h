#pragma once
#include "ShapeComponent.h"
#include "Geometry/OBB.h"

UCLASS(SpawnableComponent, DisplayName = "Box Component", Category = "Collision")
class UBoxComponent : public UShapeComponent
{
public:
	GENERATED_BODY(UBoxComponent, UShapeComponent)

	/**
	 * @brief world scale을 반영한 box half extent 반환
	 * @note 음수 scale은 절대값으로 처리
	 */
	FVector GetScaledBoxExtent() const;

	void UpdateWorldAABB() const override
	{
		const FTransform& T = GetWorldTransform();

		FVector Center = T.GetLocation();
		const FVector HalfExtent = GetScaledBoxExtent();

		// Rotation까지 포함한 conservative AABB 계산
		const FMatrix R = T.GetRotation().ToMatrix();

		FVector AbsExtent(
			std::abs(R.M[0][0]) * HalfExtent.X + std::abs(R.M[1][0]) * HalfExtent.Y + std::abs(R.M[2][0]) * HalfExtent.Z,
			std::abs(R.M[0][1]) * HalfExtent.X + std::abs(R.M[1][1]) * HalfExtent.Y + std::abs(R.M[2][1]) * HalfExtent.Z,
			std::abs(R.M[0][2]) * HalfExtent.X + std::abs(R.M[1][2]) * HalfExtent.Y + std::abs(R.M[2][2]) * HalfExtent.Z);

		WorldAABB.Min = Center - AbsExtent;
		WorldAABB.Max = Center + AbsExtent;
	}

	FOBB GetWorldOBB() const
	{
		const FTransform& T = GetWorldTransform();
		return FOBB(GetWorldLocation(), GetScaledBoxExtent(), T.GetRotation().ToMatrix());
	}

	/**
	 * @brief 이동 line과 box의 최초 접촉 결과 계산
	 */
	bool LineTraceShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionQueryParams& Params) const override;

	/**
	 * @brief 이동 sphere와 box의 최초 접촉 결과 계산
	 * @param CollisionShape 이동하는 query sphere 형상
	 * @note rounded box 대신 expanded box 보수 근사 사용
	 */
	bool SweepShape(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FCollisionShape& CollisionShape,
		const FCollisionQueryParams& Params) const override;
		
private:
	UPROPERTY(DisplayName = "Extent")
	FVector Extent = FVector(1, 1, 1);

	/**
	 * @brief local space slab 검사로 box 최초 접촉 시각 계산
	 * @param ExpandedExtent sphere sweep이면 반지름만큼 늘어난 half extent
	 */
	bool FindSegmentBoxFirstHit(
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& ExpandedExtent,
		bool bFindInitialOverlaps,
		float& OutTime,
		FVector& OutNormalLocal,
		bool& bOutStartPenetrating) const;

	/**
	 * @brief 계산된 중심 hit와 face normal을 target 표면 hit 결과로 변환
	 */
	void FillHitResult(
		FHitResult& OutHit,
		const FVector& StartWS,
		const FVector& EndWS,
		const FVector& TargetExtent,
		float Time,
		const FVector& NormalLocal,
		bool bStartPenetrating) const;

	// UShapeComponent을(를) 통해 상속됨
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	EPrimitiveType GetPrimitiveType() const override;
};
