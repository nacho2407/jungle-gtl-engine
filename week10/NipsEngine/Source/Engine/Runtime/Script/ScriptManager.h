#pragma once
#include "Core/Singleton.h"

#include "ThirdParty/sol/state.hpp"
#include <memory>
#include <Core/Containers/String.h>
#include <Object/FName.h>

class UScriptComponent;

class UScriptManager : public TSingleton<UScriptManager>
{
    friend class TSingleton<UScriptManager>;

public:
    void intializeLuaState();
    sol::state* GetGlobalLuaState() { return GLuaState.get(); }

	bool CreateScript(const FString& name);
    bool EditScript(const FString& name);

	void RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponents(const FString& name);

private:
	std::unique_ptr<sol::state> GLuaState;

	TMap<FString, TArray<UScriptComponent*>> ScriptComponentRegistry;
    TMap<FString, uint32> LastScriptWriteTime;
};
