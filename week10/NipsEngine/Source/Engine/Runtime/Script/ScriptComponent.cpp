#include "ScriptComponent.h"
#include "ScriptManager.h"

DEFINE_CLASS(UScriptComponent, UActorComponent)

void UScriptComponent::SetScript(const FString& InScriptPath)
{
	if (ScriptName == InScriptPath)
	{
		return;
	}
	ScriptName = InScriptPath;
    FScriptManager::Get().RegisterScriptComponents(ScriptName, this);
}

void UScriptComponent::OnUnregister()
{ 
	// 부모 훅 호출
    UActorComponent::OnUnregister();

    // 스크립트가 등록되어있다면 매니저에 언레지스터 요청
    if (!ScriptName.empty())
    {
        FScriptManager::Get().UnregisterScriptComponentAll(this);
        ScriptName.clear();
    }
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
	Ar << ScriptName;
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "ScriptPath", EPropertyType::String, &ScriptName });
}