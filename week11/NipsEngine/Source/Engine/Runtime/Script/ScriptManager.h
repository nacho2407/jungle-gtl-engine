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
    std::filesystem::file_time_type LastWriteTime = std::filesystem::file_time_type::min();
};

class FScriptManager : public TSingleton<FScriptManager>
{
    friend class TSingleton<FScriptManager>;
public:
    void BindMathTypes();
    void BindObjectTypes();
    void BindComponentTypes();
    void BindActorTypes();
    void BindStaticMeshTypes();
    void BindBillboardTypes();
    void BindCameraTypes();
    void BindPrimitiveTypes();
    //void BindEngineFunctions();

public:
    void initializeLuaState();
    void BindLuaState();
    void ShutdownLuaState() { GLuaState.reset(); }

    sol::state* GetGlobalLuaState() { return GLuaState.get(); }

	bool CreateScript(const FName& name);
    bool EditScript(const FName& name);
    bool HasScript(const FName& name);

	void HotReloadScripts();

	void RefreshLuaScriptFiles();
	void RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponentAll(UScriptComponent* ScriptComponent);

	std::optional<sol::environment> LoadLuaEnvironment(UScriptComponent* ScriptComponent, const FString& ScriptName);

    auto GetScriptInfo(const FName& name) -> FLuaScriptInfo*;
    auto GetScriptArray() -> TMap<FName, FLuaScriptInfo, FName::Hash>& { return ScriptArray; }

private:
	std::unique_ptr<sol::state> GLuaState;
    TMap<FName, FLuaScriptInfo, FName::Hash> ScriptArray;
};
