#pragma once

#include "Component/MovementComponent.h"
#include "Math/Rotator.h"

class URotatingMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	void TickComponent(float DeltaTime) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);
};
