#include "InputMappingContext.h"
#include "InputAction.h"

FActionKeyMapping& FInputMappingContext::AddMapping(FInputAction* Action, int32 Key)
{
	Mappings.push_back({});
	FActionKeyMapping& Mapping = Mappings.back();
	Mapping.Action = Action;
	Mapping.Key = Key;
	return Mapping;
}
