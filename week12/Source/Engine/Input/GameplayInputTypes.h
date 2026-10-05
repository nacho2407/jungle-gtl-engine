#pragma once

#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"
#include "Engine/Input/InputTypes.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"

enum class EInputActionValueType : uint8
{
	Bool,
	Axis1D,
	Axis2D,
};

enum class EInputTriggerEvent : uint8
{
	None,
	Started,
	Triggered,
	Completed,
	Canceled,
};

struct FInputActionValue
{
	EInputActionValueType Type = EInputActionValueType::Bool;
	bool BoolValue = false;
	float Axis1D = 0.0f;
	FVector2 Axis2D = FVector2::ZeroVector;

	static FInputActionValue MakeBool(bool bValue);
	static FInputActionValue MakeAxis1D(float Value);
	static FInputActionValue MakeAxis2D(const FVector2& Value);
};

struct FInputActionState
{
	FString ActionName;
	FInputActionValue Value;
	EInputTriggerEvent TriggerEvent = EInputTriggerEvent::None;
};

class FGameplayInputSnapshot
{
public:
	void Clear();
	void SetAction(const FString& ActionName, const FInputActionValue& Value, EInputTriggerEvent TriggerEvent);
	const FInputActionState* FindAction(const FString& ActionName) const;
	const TMap<FString, FInputActionState>& GetActions() const { return Actions; }

private:
	TMap<FString, FInputActionState> Actions;
};

class FDefaultGameplayInputMapping
{
public:
	static FGameplayInputSnapshot BuildSnapshot(const FInputFrame& Frame);
	static FGameplayInputSnapshot BuildSnapshot(const FInputFrame& Frame, const FInputSideEffectPermissions& Permissions);
	static FGameplayInputSnapshot BuildSnapshot(const FViewportInputContext& Context);
};
