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

//ScriptComponent에게 전달할 구조체
struct FLuaScriptLoadResult
{
    sol::environment Env;
    sol::table ScriptClass;
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
    void BindDecalTypes();
    void BindEngineAPI();
    //void BindEngineFunctions();

public:
    void initializeLuaState();
    void BindLuaState();
    void ShutdownLuaState();
    void ResetLuaState();

    sol::state* GetGlobalLuaState() { return GLuaState.get(); }

	bool CreateScript(const FName& name);
    bool EditScript(const FName& name);
    bool HasScript(const FName& name);

	bool ResolveScriptPath(const FString& ScriptName, FString& OutPath);

	void HotReloadScripts();

	void RefreshLuaScriptFiles();
	void RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent);
    void UnregisterScriptComponentAll(UScriptComponent* ScriptComponent);

    std::optional<FLuaScriptLoadResult> LoadScriptClass(
        UScriptComponent* Component,
        const FString& ScriptName);

    std::optional<sol::table> LoadScriptClassForProperties(
        const FString& ScriptName);

    FWString GetScriptPathByName(const FName& name);
    auto GetScriptInfo(const FName& name) -> FLuaScriptInfo*;
    auto GetScriptArray() -> TMap<FName, FLuaScriptInfo, FName::Hash>& { return ScriptArray; }

private:
    void ConfigureLuaPackagePath();

	std::unique_ptr<sol::state> GLuaState;
    TMap<FName, FLuaScriptInfo, FName::Hash> ScriptArray;
};
