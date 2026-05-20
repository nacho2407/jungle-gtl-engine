#include "InputComponent.h"

#include "Core/Log.h"
#include "Input/InputSystem.h"
#include "Object/Reflection/ObjectFactory.h"
void UInputComponent::AddAxisMapping(const FString& Name, int VKey, float Scale)
{
	FAxisMapping M;
	M.Name  = Name;
	M.VKey  = VKey;
	M.Scale = Scale;
	AxisMappings.push_back(std::move(M));
}

void UInputComponent::AddActionMapping(const FString& Name, int VKey)
{
	FActionMapping M;
	M.Name = Name;
	M.VKey = VKey;
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

void UInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const InputSystem& In = InputSystem::Get();

	// Axis: 매핑 평가 → name 별 합산 → 매칭 binding 호출.
	// UE 와 동일 — 매 frame 호출 (value=0 도 호출됨, 자식이 0 분기 처리).
	for (const FAxisBinding& B : AxisBindings)
	{
		float Value = 0.0f;
		for (const FAxisMapping& M : AxisMappings)
		{
			if (M.Name == B.Name && In.GetKey(M.VKey))
			{
				Value += M.Scale;
			}
		}
		if (B.Callback) B.Callback(Value);
	}

	// Action: edge 감지 (Pressed = KeyDown, Released = KeyUp).
	for (const FActionBinding& B : ActionBindings)
	{
		for (const FActionMapping& M : ActionMappings)
		{
			if (M.Name != B.Name) continue;
			const bool bFired = (B.Event == EInputEvent::Pressed)
				? In.GetKeyDown(M.VKey)
				: In.GetKeyUp(M.VKey);
			if (bFired && B.Callback)
			{
				B.Callback();
				break;  // 같은 action 의 여러 매핑이 같은 frame 발화해도 1회만.
			}
		}
	}
}
