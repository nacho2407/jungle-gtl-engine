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
    TArray<std::shared_ptr<UScriptComponent>> ScriptComponents;
    std::filesystem::file_time_type LastWriteTime;
};

class UScriptManager : public TSingleton<UScriptManager>
{
    friend class TSingleton<UScriptManager>;

public:
    void intializeLuaState();
    void BindLuaState();
    void ShutdownLuaState() { GLuaState.reset(); }

	void ReloadAllScripts();
    sol::state* GetGlobalLuaState() { return GLuaState.get(); }

	bool CreateScript(const FName& name);
    bool EditScript(const FName& name);

	void RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponents(const FString& name);

private:
	std::unique_ptr<sol::state> GLuaState;
    TMap<FName, FLuaScriptInfo, FName::Hash> ScriptArray;
};
