#pragma once
#include "Core/Singleton.h"

#include "ThirdParty/sol/state.hpp"
#include <memory>
#include <Core/Containers/String.h>
#include <Object/FName.h>
#include <filesystem>

class UScriptComponent;

struct FLuaScriptInfo
{
    FWString ScriptPath;
    TArray<UScriptComponent*> ScriptComponents;
    std::filesystem::file_time_type LastWriteTime;
};

class FScriptManager : public TSingleton<FScriptManager>
{
    friend class TSingleton<FScriptManager>;

public:
    void initializeLuaState();
    void CheckAllLuaScripts();
    void BindLuaState();
    void ShutdownLuaState() { GLuaState.reset(); }

	void ReloadAllScripts();
    sol::state* GetGlobalLuaState() { return GLuaState.get(); }

	bool CreateScript(const FName& name);
    bool EditScript(const FName& name);

	void RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponentAll(UScriptComponent* ScriptComponent);

    auto GetScriptInfo(const FName& name) -> FLuaScriptInfo*;
    auto GetScriptArray() -> TMap<FName, FLuaScriptInfo, FName::Hash>& { return ScriptArray; }

private:
	std::unique_ptr<sol::state> GLuaState;
    TMap<FName, FLuaScriptInfo, FName::Hash> ScriptArray;
};
