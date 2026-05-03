#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegates/Delegate.h"
#include "Input/InputTypes.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "LuaScript/LuaIncludes.h"
#include "LuaScript/LuaWorldProxy.h"

class ULuaScriptComponent : public UActorComponent
{
public:
    DECLARE_CLASS(ULuaScriptComponent, UActorComponent)

    void BeginPlay() override;
    void EndPlay() override;

    bool HasScript() const { return !(LuaScriptPath.empty() || LuaScriptPath == "None"); }

    void SetScriptPath(const FString& ScriptPath);
    const FString& GetScriptPath() const { return LuaScriptPath; }

    void StartCoroutine(const FString& FunctionName, sol::variadic_args Args);
    bool CallFunction(const FString& FunctionName, sol::variadic_args Args);

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
    void BindNativeDelegates();
    void UnbindNativeDelegates();

	void DispatchInputEvents();
    void DispatchVirtualKeyEvents(const FInputSnapshot& Input);
    void DispatchKeyboardEvent(const FInputSnapshot& Input, int32 VK);
    void DispatchMouseButtonEvent(const FInputSnapshot& Input, int32 VK);
    void DispatchGamepadEvents(const FInputSnapshot& Input);

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
    FLuaWorldProxy WorldProxy;

	// Input Delegate 함수들
	sol::protected_function OnKeyPressedFunc;
    sol::protected_function OnKeyReleasedFunc;


	sol::protected_function OnMouseButtonPressedFunc;
    sol::protected_function OnMouseButtonReleasedFunc;

    sol::protected_function OnGamepadButtonPressedFunc;
    sol::protected_function OnGamepadButtonReleasedFunc;

    sol::protected_function OnOverlapBeginFunc;
    sol::protected_function OnOverlapEndFunc;
    sol::protected_function OnTakeDamageFunc;

    DelegateHandle OnOverlapBeginHandle = 0;
    DelegateHandle OnOverlapEndHandle = 0;
    DelegateHandle OnTakeDamageHandle = 0;
};
