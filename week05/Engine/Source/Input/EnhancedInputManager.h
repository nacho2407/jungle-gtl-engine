#pragma once
#include "CoreMinimal.h"
#include "InputTrigger.h"
#include <functional>

class CInputManager;

struct FInputMappingContext;
struct FInputAction;

using FInputActionCallback = std::function<void(const FInputActionValue&)>;
class ENGINE_API  CEnhancedInputManager
{
public:

	void AddMappingContext(FInputMappingContext* Context, int32 Priority = 0);
	void RemoveMappingContext(FInputMappingContext* Context);
	void ClearAllMappingContexts();

	void BindAction(FInputAction* Action, ETriggerEvent TriggerEvent, FInputActionCallback Callback);
	void ClearBindings();

	void ProcessInput(CInputManager* RawInput, float DeltaTime);
private:
	FInputActionValue GetRawActionValue(CInputManager* Input, int32 Key);
	struct FMappingContextEntry
	{
		FInputMappingContext* Context;
		int32 Priority;
	};

	struct FBindingEntry
	{
		FInputAction* Action;
		ETriggerEvent TriggerEvent;
		FInputActionCallback Callback;
	};
	TArray<FMappingContextEntry> MappingContexts;
	TArray<FBindingEntry> Bindings;
	TMap<FInputAction*, ETriggerState> ActionStates;
};