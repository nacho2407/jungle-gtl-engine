#pragma once

#include "MovementComponent.h"

#include <algorithm>

class UPrimitiveComponent;
class USceneComponent;

class UFloatingPawnMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UFloatingPawnMovementComponent, UMovementComponent)
	static void RegisterProperties(UClass* Class);

	UFloatingPawnMovementComponent() = default;
	~UFloatingPawnMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetMoveInput(float ForwardValue, float RightValue)
	{
		MoveInput = std::max<float>(-1.0f, std::min<float>(1.0f, ForwardValue));
		RightMoveInput = std::max<float>(-1.0f, std::min<float>(1.0f, RightValue));
	}
	void SetLookInput(float DeltaX, float DeltaY)
	{
		LookInputX += DeltaX;
		LookInputY += DeltaY;
	}

private:
	UPrimitiveComponent* UpdatedPrimitive = nullptr;

	float MoveInput = 0.0f;
	float RightMoveInput = 0.0f;
	float LookInputX = 0.0f;
	float LookInputY = 0.0f;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float Speed = 10.0f;
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="MouseSensitivity", Min=0.0f, Max=10.0f, Speed=0.01f)
	float MouseSensitivity = 0.1f;
};
