#pragma once
#include "CoreMinimal.h"

class FInputModifier;
class FInputTrigger;
struct FInputAction;

struct ENGINE_API FActionKeyMapping
{
	FInputAction* Action = nullptr;
	int32 Key = 0;
	TArray<FInputTrigger*> Triggers;  
	TArray<FInputModifier*> Modifiers;
};
struct ENGINE_API FInputMappingContext
{
	FString ContextName;
	TArray<FActionKeyMapping> Mappings;

	FActionKeyMapping& AddMapping(FInputAction* Action, int32 Key);

};