#include "Engine/Input/GameplayInputTypes.h"

#include <initializer_list>
#include <windows.h>

namespace
{
	EInputTriggerEvent GetDigitalTrigger(const FInputFrame& Frame, std::initializer_list<int> Keys)
	{
		bool bPressed = false;
		bool bDown = false;
		bool bReleased = false;
		for (int Key : Keys)
		{
			bPressed = bPressed || Frame.WasPressed(Key);
			bDown = bDown || Frame.IsDown(Key);
			bReleased = bReleased || Frame.WasReleased(Key);
		}

		if (bPressed)
		{
			return EInputTriggerEvent::Started;
		}
		if (bDown)
		{
			return EInputTriggerEvent::Triggered;
		}
		if (bReleased)
		{
			return EInputTriggerEvent::Completed;
		}
		return EInputTriggerEvent::None;
	}

	bool IsDigitalActive(EInputTriggerEvent TriggerEvent)
	{
		return TriggerEvent == EInputTriggerEvent::Started
			|| TriggerEvent == EInputTriggerEvent::Triggered;
	}
}

FInputActionValue FInputActionValue::MakeBool(bool bValue)
{
	FInputActionValue Result;
	Result.Type = EInputActionValueType::Bool;
	Result.BoolValue = bValue;
	Result.Axis1D = bValue ? 1.0f : 0.0f;
	Result.Axis2D = bValue ? FVector2(1.0f, 0.0f) : FVector2::ZeroVector;
	return Result;
}

FInputActionValue FInputActionValue::MakeAxis1D(float Value)
{
	FInputActionValue Result;
	Result.Type = EInputActionValueType::Axis1D;
	Result.BoolValue = Value != 0.0f;
	Result.Axis1D = Value;
	Result.Axis2D = FVector2(Value, 0.0f);
	return Result;
}

FInputActionValue FInputActionValue::MakeAxis2D(const FVector2& Value)
{
	FInputActionValue Result;
	Result.Type = EInputActionValueType::Axis2D;
	Result.BoolValue = Value.X != 0.0f || Value.Y != 0.0f;
	Result.Axis1D = Value.X;
	Result.Axis2D = Value;
	return Result;
}

void FGameplayInputSnapshot::Clear()
{
	Actions.clear();
}

void FGameplayInputSnapshot::SetAction(
	const FString& ActionName,
	const FInputActionValue& Value,
	EInputTriggerEvent TriggerEvent)
{
	FInputActionState State;
	State.ActionName = ActionName;
	State.Value = Value;
	State.TriggerEvent = TriggerEvent;
	Actions[ActionName] = State;
}

const FInputActionState* FGameplayInputSnapshot::FindAction(const FString& ActionName) const
{
	auto It = Actions.find(ActionName);
	return It != Actions.end() ? &It->second : nullptr;
}

FGameplayInputSnapshot FDefaultGameplayInputMapping::BuildSnapshot(const FInputFrame& Frame)
{
	return BuildSnapshot(Frame, FInputSideEffectPermissions{});
}

FGameplayInputSnapshot FDefaultGameplayInputMapping::BuildSnapshot(
	const FInputFrame& Frame,
	const FInputSideEffectPermissions& Permissions)
{
	FGameplayInputSnapshot Snapshot;

	const FVector2 MoveAxis(
		(Permissions.bAllowGameMove && Frame.IsDown('D') ? 1.0f : 0.0f)
			- (Permissions.bAllowGameMove && Frame.IsDown('A') ? 1.0f : 0.0f),
		(Permissions.bAllowGameMove && Frame.IsDown('W') ? 1.0f : 0.0f)
			- (Permissions.bAllowGameMove && Frame.IsDown('S') ? 1.0f : 0.0f));
	const EInputTriggerEvent MoveTrigger = Permissions.bAllowGameMove && (MoveAxis.X != 0.0f || MoveAxis.Y != 0.0f)
		? EInputTriggerEvent::Triggered
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Move", FInputActionValue::MakeAxis2D(MoveAxis), MoveTrigger);

	const float MoveVerticalAxis =
		(Permissions.bAllowGameMove && (Frame.IsDown(VK_SPACE) || Frame.IsDown('E')) ? 1.0f : 0.0f)
		- (Permissions.bAllowGameMove && (Frame.IsDown(VK_CONTROL) || Frame.IsDown('Q')) ? 1.0f : 0.0f);
	const EInputTriggerEvent MoveVerticalTrigger = Permissions.bAllowGameMove && MoveVerticalAxis != 0.0f
		? EInputTriggerEvent::Triggered
		: EInputTriggerEvent::None;
	Snapshot.SetAction("MoveVertical", FInputActionValue::MakeAxis1D(MoveVerticalAxis), MoveVerticalTrigger);

	const FVector2 LookAxis(
		Permissions.bAllowGameLook ? static_cast<float>(Frame.MouseDelta.x) : 0.0f,
		Permissions.bAllowGameLook ? static_cast<float>(Frame.MouseDelta.y) : 0.0f);
	const EInputTriggerEvent LookTrigger = Permissions.bAllowGameLook && (LookAxis.X != 0.0f || LookAxis.Y != 0.0f)
		? EInputTriggerEvent::Triggered
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Look", FInputActionValue::MakeAxis2D(LookAxis), LookTrigger);

	const EInputTriggerEvent AttackTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_LBUTTON })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Attack", FInputActionValue::MakeBool(IsDigitalActive(AttackTrigger)), AttackTrigger);

	const EInputTriggerEvent DashTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_SHIFT, VK_RBUTTON })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Dash", FInputActionValue::MakeBool(IsDigitalActive(DashTrigger)), DashTrigger);

	const EInputTriggerEvent InteractTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { 'E' })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Interact", FInputActionValue::MakeBool(IsDigitalActive(InteractTrigger)), InteractTrigger);

	const EInputTriggerEvent JumpTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_SPACE })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Jump", FInputActionValue::MakeBool(IsDigitalActive(JumpTrigger)), JumpTrigger);

	const EInputTriggerEvent PauseTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_ESCAPE, 'Q' })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Pause", FInputActionValue::MakeBool(IsDigitalActive(PauseTrigger)), PauseTrigger);

	const EInputTriggerEvent ConfirmTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_RETURN, VK_SPACE, VK_LBUTTON })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Confirm", FInputActionValue::MakeBool(IsDigitalActive(ConfirmTrigger)), ConfirmTrigger);

	const EInputTriggerEvent CancelTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { VK_ESCAPE, VK_RBUTTON })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("Cancel", FInputActionValue::MakeBool(IsDigitalActive(CancelTrigger)), CancelTrigger);

	const EInputTriggerEvent DebugRestartTrigger = Permissions.bAllowGameActions
		? GetDigitalTrigger(Frame, { 'R' })
		: EInputTriggerEvent::None;
	Snapshot.SetAction("DebugRestart", FInputActionValue::MakeBool(IsDigitalActive(DebugRestartTrigger)), DebugRestartTrigger);

	return Snapshot;
}

FGameplayInputSnapshot FDefaultGameplayInputMapping::BuildSnapshot(const FViewportInputContext& Context)
{
	return BuildSnapshot(Context.Frame, Context.SideEffects);
}
