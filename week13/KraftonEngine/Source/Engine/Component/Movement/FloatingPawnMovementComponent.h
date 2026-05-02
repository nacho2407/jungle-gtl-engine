#pragma once

#include "MovementComponent.h"

#include <algorithm>

class UPrimitiveComponent;

class UFloatingPawnMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UFloatingPawnMovementComponent, UMovementComponent)

	UFloatingPawnMovementComponent() = default;
	~UFloatingPawnMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetMoveInput(float Value) { MoveInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }
	void SetRotationInput(float Value) { RotationInput = std::max<float>(-1.0f, std::min<float>(1.0f, Value)); }

private:
	UPrimitiveComponent* UpdatedPrimitive = nullptr;

	float MoveInput = 0.0f;
	float RotationInput = 0.0f;

	float Speed = 5.0f;
	float RotationSpeed = 30.0f;
};
