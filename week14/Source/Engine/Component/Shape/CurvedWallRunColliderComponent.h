#pragma once

#include "Component/ShapeComponent.h"

#include "Source/Engine/Component/Shape/CurvedWallRunColliderComponent.generated.h"

/**
 * @brief 곡선형 wall-run query collider 컴포넌트
 */
UCLASS()
class UCurvedWallRunColliderComponent : public UShapeComponent
{
public:
	GENERATED_BODY()

	UCurvedWallRunColliderComponent();

	/**
	 * @brief 곡선 collider의 기본 형상 값을 한 번에 설정합니다
	 *
	 * @param InRadius 원호 중심에서 벽 중앙선까지의 반지름
	 *
	 * @param InHalfHeight 벽의 절반 높이
	 *
	 * @param InThickness 벽의 두께
	 *
	 * @param InArcAngleDegrees 원호가 차지하는 각도
	 *
	 * @param InSegmentCount 물리 패널과 query 근사에 사용할 분할 수
	 */
	void SetCurveShape(float InRadius, float InHalfHeight, float InThickness, float InArcAngleDegrees, int32 InSegmentCount);

	float GetRadius() const { return Radius; }
	float GetHalfHeight() const { return HalfHeight; }
	float GetThickness() const { return Thickness; }
	float GetArcAngleDegrees() const { return ArcAngleDegrees; }
	int32 GetSegmentCount() const { return SegmentCount; }
	bool ShouldEnablePhysicalPanels() const { return bEnablePhysicalPanels; }

	float GetScaledRadius() const;
	float GetScaledHalfHeight() const;
	float GetScaledThickness() const;
	float GetArcAngleRadians() const;
	int32 GetSafeSegmentCount() const;

	void UpdateWorldAABB() const override;
	void PostEditProperty(const char* PropertyName) override;

private:
	/**
	 * @brief 저장된 형상 값을 안전 범위로 보정합니다
	 */
	void SanitizeShapeValues();

protected:
	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Radius", Min=0.05f, Max=10000.0f, Speed=0.1f)
	float Radius = 8.0f;

	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Half Height", Min=0.05f, Max=10000.0f, Speed=0.1f)
	float HalfHeight = 3.0f;

	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Thickness", Min=0.02f, Max=10000.0f, Speed=0.05f)
	float Thickness = 0.5f;

	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Arc Angle Degrees", Min=1.0f, Max=360.0f, Speed=1.0f)
	float ArcAngleDegrees = 90.0f;

	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Segment Count", Min=1, Max=128, Speed=1)
	int32 SegmentCount = 16;

	UPROPERTY(Edit, Save, Category="Curved Wall", DisplayName="Enable Physical Panels")
	bool bEnablePhysicalPanels = true;
};
