#include "InputComponent.h"

#include "Core/Logging/Log.h"
#include "Input/InputSystem.h"
#include "Object/Reflection/ObjectFactory.h"

void UInputComponent::AddAxisMapping(const FString& Name, int InputCode, float Scale)
{
	FAxisMapping M;
	M.Name      = Name;
	M.InputCode = InputCode;
	M.Scale = Scale;
	AxisMappings.push_back(std::move(M));
}

void UInputComponent::AddActionMapping(const FString& Name, int InputCode)
{
	FActionMapping M;
	M.Name      = Name;
	M.InputCode = InputCode;
	ActionMappings.push_back(std::move(M));
}

void UInputComponent::BindAxis(const FString& Name, TFunction<void(float)> Callback)
{
	FAxisBinding B;
	B.Name     = Name;
	B.Callback = std::move(Callback);
	AxisBindings.push_back(std::move(B));
}

void UInputComponent::BindAction(const FString& Name, EInputEvent Event, TFunction<void()> Callback)
{
	FActionBinding B;
	B.Name     = Name;
	B.Event    = Event;
	B.Callback = std::move(Callback);
	ActionBindings.push_back(std::move(B));
}

void UInputComponent::ClearBindings()
{
	AxisBindings.clear();
	ActionBindings.clear();
}

void UInputComponent::ClearMappings()
{
	AxisMappings.clear();
	ActionMappings.clear();
}

void UInputComponent::ClearAllMappingsAndBindings()
{
	ClearMappings();
	ClearBindings();
}

void UInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const InputSystem& In = InputSystem::Get();
	if (In.IsGuiUsingMouse())
	{
		return;
	}

	// Axis mapping 합산 — 키/버튼은 0 또는 1, 게임패드 축은 정규화된 아날로그 값
	for (const FAxisBinding& B : AxisBindings)
	{
		float Value = 0.0f;
		for (const FAxisMapping& M : AxisMappings)
		{
			if (M.Name == B.Name)
			{
				Value += In.GetAxisValue(M.InputCode) * M.Scale;
			}
		}
		if (B.Callback) B.Callback(Value);
	}

	// Action edge 감지 — 같은 action의 여러 mapping이 동시에 발화해도 callback은 1회만 호출
	for (const FActionBinding& B : ActionBindings)
	{
		for (const FActionMapping& M : ActionMappings)
		{
			if (M.Name != B.Name) continue;
			const bool bFired = (B.Event == EInputEvent::Pressed)
				? In.GetKeyDown(M.InputCode)
				: In.GetKeyUp(M.InputCode);
			if (bFired && B.Callback)
			{
				B.Callback();
				break;
			}
		}
	}
}
