#pragma once
#include "ActorComponent.h"

class ULuaScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(ULuaScriptComponent, UActorComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	bool HasScript() const { return !LuaScriptFilePath.empty(); }

	void SetScriptFileName(const FString& Neaname) { LuaScriptFilePath = Neaname; }
	const FString& GetScriptFileName() { return LuaScriptFilePath; }

	void clearScript() { LuaScriptFilePath.clear(); }

private:
	FString LuaScriptFilePath;
};