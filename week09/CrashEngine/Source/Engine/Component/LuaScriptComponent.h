#pragma once
#include "ActorComponent.h"

class ULuaScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(ULuaScriptComponent, UActorComponent)

	void BeginPlay() override;
	void EndPlay() override;

	bool HasScript() const { return !(LuaScriptPath.empty() || LuaScriptPath == "None"); }

	void SetScriptPath(const FString& ScriptPath) { LuaScriptPath = ScriptPath; }
	const FString& GetScriptPath() { return LuaScriptPath; }

	void clearScript() { LuaScriptPath = "None"; }

	void ReloadScript();

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	FString LuaScriptPath = "None";
};