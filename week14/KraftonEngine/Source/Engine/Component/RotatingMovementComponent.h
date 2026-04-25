#pragma once

#include "MovementComponent.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

// 런타임 동안 UpdatedComponent를 일정 각속도로 회전시키는 이동 컴포넌트
class URotatingMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	URotatingMovementComponent() = default;
	~URotatingMovementComponent() override = default;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

private:
	FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	bool bRotationInLocalSpace = false;
	FVector PivotTranslation = FVector(0.0f, 0.0f, 0.0f);

	// World-space 공전 모드에서 고정 pivot을 유지하기 위한 런타임 캐시
	USceneComponent* CachedWorldPivotComponent = nullptr;
	FVector CachedWorldPivotTranslation = FVector(0.0f, 0.0f, 0.0f);
	FVector CachedWorldPivotLocation = FVector(0.0f, 0.0f, 0.0f);
	bool bWorldPivotInitialized = false;
};
