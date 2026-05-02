#pragma once

#include "Component/ActorComponent.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "LuaScript/LuaIncludes.h"

class ULuaScriptComponent : public UActorComponent
{
public:
    DECLARE_CLASS(ULuaScriptComponent, UActorComponent)

    void BeginPlay() override;
    void EndPlay() override;

    bool HasScript() const { return !(LuaScriptPath.empty() || LuaScriptPath == "None"); }

    void SetScriptPath(const FString& ScriptPath);
    const FString& GetScriptPath() const { return LuaScriptPath; }

    void ClearScript();

    bool LoadScript();
    bool ReloadScript();

    const FString& GetLastError() const { return LastError; }
    bool IsScriptLoaded() const { return bScriptLoaded; }

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    void ClearScriptRuntime();
    void CacheScriptFunctions();
    void SetLastError(const FString& InError);

private:
    FString LuaScriptPath = "None";
    FString LastError;

    bool bScriptLoaded = false;

	// sol::state는 LuaRuntime에서 하나만 관리하고, 각 스크립트 컴포넌트는 sol::environment을 사용하여 격리된 Lua 환경을 가짐
    sol::environment Env;
    sol::protected_function BeginPlayFunc;
    sol::protected_function TickFunc;
    sol::protected_function EndPlayFunc;

    FLuaGameObjectProxy ObjProxy;
};