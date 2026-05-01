#pragma once
#include "Component/ActorComponent.h"
#include "ThirdParty/sol/sol.hpp"

class UScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UScriptComponent, UActorComponent)
	UScriptComponent() = default;
	~UScriptComponent() override = default;

	void PostDuplicate(UObject* Original) override;
	void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	void SetScript(const FString& InScriptPath);
    const FString& GetScriptPath() const { return ScriptPath; }

private:
    FString ScriptPath;
    sol::environment ScriptEnv;
    sol::table ScriptTable;
};
