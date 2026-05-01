#include "ScriptComponent.h"
#include "ScriptManager.h"

void UScriptComponent::SetScript(const FString& InScriptPath)
{
	if (ScriptPath == InScriptPath)
	{
		return;
	}
	ScriptPath = InScriptPath;
    UScriptManager::Get().RegisterScriptComponents(ScriptPath, this);
}

void UScriptComponent::PostDuplicate(UObject* Original)
{
    UActorComponent::PostDuplicate(Original);
	UScriptComponent* OriginalScriptComp = Cast<UScriptComponent>(Original);
	if (OriginalScriptComp)
	{
		SetScript(OriginalScriptComp->GetScriptPath());
	}
}

void UScriptComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << ScriptPath;
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "ScriptPath", EPropertyType::String, &ScriptPath });
}