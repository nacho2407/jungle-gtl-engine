#pragma once
#include "InputAction.h"

enum class ETriggerState : uint32
{
	None,
	Ongoing,
	Triggered,
};

enum class ETriggerEvent : uint32
{
	None = 0,
	Started = 1 << 0,
	Ongoing = 1 << 1,
	Triggered = 1 << 2,
	Completed = 1 << 3,
	Canceled = 1 << 4,
};
inline ETriggerEvent operator|(ETriggerEvent A, ETriggerEvent B)
{
	return static_cast<ETriggerEvent>(static_cast<uint32>(A) | static_cast<uint32>(B));
}
inline bool operator&(ETriggerEvent A, ETriggerEvent B)
{
	return (static_cast<uint32>(A) & static_cast<uint32>(B)) != 0;
}
class ENGINE_API FInputTrigger
{
public:
	virtual ~FInputTrigger() = default;
	virtual ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) = 0;
	virtual void Reset() { LastState = ETriggerState::None; }

	ETriggerState LastState = ETriggerState::None;
};
class ENGINE_API FTriggerDown : public FInputTrigger//눌린동안
{
public:
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = Value.IsNonZero() ? ETriggerState::Triggered : ETriggerState::None;
		LastState = State;
		return State;
	}
};
class ENGINE_API FTriggerPressed : public FInputTrigger// 눌린 1회
{
public:
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = ETriggerState::None;
		if (Value.IsNonZero() && LastState == ETriggerState::None)
			State = ETriggerState::Triggered;
		else if (Value.IsNonZero())
			State = ETriggerState::Ongoing;

		LastState = Value.IsNonZero() ? ETriggerState::Ongoing : ETriggerState::None;
		return State;
	}
};

class ENGINE_API FInputReleased : public FInputTrigger //뗀 1회
{
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		ETriggerState State = ETriggerState::None;
		if (!Value.IsNonZero() && LastState != ETriggerState::None)
			State = ETriggerState::Triggered;
		else if (Value.IsNonZero())
			State = ETriggerState::Ongoing;

		LastState = Value.IsNonZero() ? ETriggerState::Ongoing : ETriggerState::None;
		return State;
	}
};

class ENGINE_API FInputHold : public FInputTrigger
{
	float HoldTimeThreshold = 0.5f;
	float HeldDuration = 0.0f;
	bool bTriggered = false;
	ETriggerState UpdateState(const FInputActionValue& Value, float DeltaTime) override
	{
		if (!Value.IsNonZero())
		{
			Reset();
			return ETriggerState::None;
		}
		HeldDuration += DeltaTime;
		if (HeldDuration >= HoldTimeThreshold)
		{
			bTriggered = true;
			LastState = ETriggerState::Triggered;
			return ETriggerState::Triggered;
		}

		LastState = ETriggerState::Ongoing;
		return ETriggerState::Ongoing;
	}
	void Reset() override
	{
		FInputTrigger::Reset();
		HeldDuration = 0.0f;
		bTriggered = false;
	}
};

