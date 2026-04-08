#pragma once

#include "MovementComponent.h"
#include "Math/Rotator.h"

// 런타임 동안 UpdatedComponent를 일정 각속도로 회전시키는 이동 컴포넌트
class URotatingMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	URotatingMovementComponent() = default;
	~URotatingMovementComponent() override = default;

	void TickComponent(float DeltaTime) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

private:
	FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);
};
