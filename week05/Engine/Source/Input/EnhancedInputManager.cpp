#include "EnhancedInputManager.h"
#include "InputModifier.h"
#include "InputManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include <algorithm>

void FEnhancedInputManager::AddMappingContext(FInputMappingContext* Context, int32 Priority)
{
	MappingContexts.push_back({ Context, Priority });
	std::sort(MappingContexts.begin(), MappingContexts.end(),
		[](const FMappingContextEntry& A, const FMappingContextEntry& B)
	{
		return A.Priority > B.Priority;
	});
}

void FEnhancedInputManager::RemoveMappingContext(FInputMappingContext* Context)
{
	MappingContexts.erase(
		std::remove_if(MappingContexts.begin(), MappingContexts.end(),
			[Context](const FMappingContextEntry& E) { return E.Context == Context; }),
		MappingContexts.end());
}

void FEnhancedInputManager::ClearAllMappingContexts()
{
	MappingContexts.clear();
}

void FEnhancedInputManager::BindAction(FInputAction* Action, ETriggerEvent TriggerEvent, FInputActionCallback Callback)
{
	Bindings.push_back({ Action, TriggerEvent, std::move(Callback) });
}

void FEnhancedInputManager::ClearBindings()
{
	Bindings.clear();
}


FInputActionValue FEnhancedInputManager::GetRawActionValue(FInputManager* Input, int32 Key)
{
	if (Key == static_cast<int32>(EInputKey::MouseX))
		return FInputActionValue(Input->GetMouseDeltaX());
	if (Key == static_cast<int32>(EInputKey::MouseY))
		return FInputActionValue(Input->GetMouseDeltaY());
	return FInputActionValue(Input->IsKeyDown(Key) ? 1.0f : 0.0f);
}

void FEnhancedInputManager::ProcessInput(FInputManager* RawInput, float DeltaTime)
{
	TMap<FInputAction*, FInputActionValue> ActionValues;
	TMap<FInputAction*, ETriggerState> NewTriggerStates;
	for (const FMappingContextEntry& ContextEntry : MappingContexts)
	{
		for (FActionKeyMapping& Mapping : ContextEntry.Context->Mappings)
		{
			if (!Mapping.Action)
				continue;

			FInputActionValue Value = GetRawActionValue(RawInput, Mapping.Key);

			for (auto& Modifier : Mapping.Modifiers)
			{
				Value = Modifier->ModifyRaw(Value);
			}
			ETriggerState MappingTriggerState = ETriggerState::None;

			if (Mapping.Triggers.empty())
			{
				MappingTriggerState = Value.IsNonZero()
					? ETriggerState::Triggered
					: ETriggerState::None;
			}
			else
			{
				bool bAllTriggered = true;
				bool bAnyOngoing = false;
				for (auto& Trigger : Mapping.Triggers)
				{
					ETriggerState State = Trigger->UpdateState(Value, DeltaTime);
					if (State != ETriggerState::Triggered)
						bAllTriggered = false;
					if (State == ETriggerState::Ongoing)
						bAnyOngoing = true;
				}

				if (bAllTriggered)
					MappingTriggerState = ETriggerState::Triggered;
				else if (bAnyOngoing)
					MappingTriggerState = ETriggerState::Ongoing;
			}

			if (ActionValues.find(Mapping.Action) == ActionValues.end())
				ActionValues[Mapping.Action] = FInputActionValue();

			ActionValues[Mapping.Action] = ActionValues[Mapping.Action] + Value;

			ETriggerState& CurrentBest = NewTriggerStates[Mapping.Action];
			if (static_cast<uint32>(MappingTriggerState) > static_cast<uint32>(CurrentBest))
				CurrentBest = MappingTriggerState;
		}
	}


	// 2. 상태 전이 → ETriggerEvent 결정 + Binding 호출
	for (auto& [Action, NewState] : NewTriggerStates)
	{
		ETriggerState PrevState = ActionStates[Action];

		ETriggerEvent Event = ETriggerEvent::None;
		if (NewState == ETriggerState::Triggered)
		{
			if (PrevState == ETriggerState::None)
				Event = ETriggerEvent::Started | ETriggerEvent::Triggered;
			else
				Event = ETriggerEvent::Triggered;
		}
		else if (NewState == ETriggerState::Ongoing)
		{
			if (PrevState == ETriggerState::None)
				Event = ETriggerEvent::Started | ETriggerEvent::Ongoing;
			else
				Event = ETriggerEvent::Ongoing;
		}
		else // NewState == None
		{
			if (PrevState == ETriggerState::Triggered)
				Event = ETriggerEvent::Completed;
			else if (PrevState == ETriggerState::Ongoing)
				Event = ETriggerEvent::Canceled;
		}

		ActionStates[Action] = NewState;

		// 매칭 Binding 호출
		if (Event != ETriggerEvent::None)
		{
			FInputActionValue& ActionValue = ActionValues[Action];
			for (const FBindingEntry& Binding : Bindings)
			{
				if (Binding.Action == Action && (Event & Binding.TriggerEvent))
				{
					Binding.Callback(ActionValue);
				}
			}
		}
	}

	// 3. 이번 프레임에 매핑이 없었던 Action의 상태 정리
	for (auto It = ActionStates.begin(); It != ActionStates.end(); ++It)
	{
		if (NewTriggerStates.find(It->first) == NewTriggerStates.end())
		{
			if (It->second != ETriggerState::None)
			{
				// 매핑이 사라진 Action → Completed/Canceled
				ETriggerEvent Event = (It->second == ETriggerState::Triggered)
					? ETriggerEvent::Completed
					: ETriggerEvent::Canceled;

				FInputActionValue ZeroValue;
				for (const FBindingEntry& Binding : Bindings)
				{
					if (Binding.Action == It->first && (Event & Binding.TriggerEvent))
					{
						Binding.Callback(ZeroValue);
					}
				}
			}
			It->second = ETriggerState::None;
		}
	}
}