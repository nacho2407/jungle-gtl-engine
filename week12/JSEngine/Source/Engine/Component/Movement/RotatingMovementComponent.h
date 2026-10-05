#pragma once

#include "MovementComponent.h"

UCLASS(SpawnableComponent, DisplayName = "RotatingMovement Component", Category = "Movement")
class URotatingMovementComponent : public UMovementComponent
{	
public:
	GENERATED_BODY(URotatingMovementComponent, UMovementComponent)

	virtual void TickComponent(float DeltaTime) override;

	// RotationRate: X=X축 각속도, Y=Y축 각속도, Z=Z축 각속도 (deg/s)
	void SetRotationSpeed(const FVector& InRotationRate) { RotationRate = InRotationRate; }
	const FVector& GetRotationRate() const { return RotationRate; }

	void SetPivotTranslation(const FVector& InPivot) { PivotTranslation = InPivot; }
	const FVector& GetPivotTranslation() const { return PivotTranslation; }

	void SetRotationInLocalSpace(bool bInLocalSpace) { bRotationInLocalSpace = bInLocalSpace; }
	bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }

	virtual float GetMaxSpeed() const override { return 0.0f; } // 회전 컴포넌트이므로 0.0f 반환

private:
	// X=X축(Roll), Y=Y축(Pitch), Z=Z축(Yaw) 각속도 (deg/s)
	UPROPERTY(DisplayName = "Rotation Rate", Min = -360.0f, Max = 360.0f, Speed = 1.0f, LuaReadWrite, LuaName = RotationRate)
	FVector RotationRate = FVector(90.0f, 0.f, 0.f);

	// 오브젝트 로컬 공간 기준 피벗 오프셋 (오브젝트 → 피벗 방향)
	UPROPERTY(DisplayName = "Pivot Translation", Speed = 0.1f, LuaReadWrite, LuaName = PivotTranslation)
	FVector PivotTranslation = FVector::ZeroVector;
	
	UPROPERTY(DisplayName = "Local Space Rotation", LuaReadWrite, LuaName = RotationInLocalSpace)
	bool bRotationInLocalSpace = true;
};
