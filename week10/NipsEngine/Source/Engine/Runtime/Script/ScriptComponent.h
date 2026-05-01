#pragma once
#include "Component/ActorComponent.h"
#include "Core/Logging/Log.h"
#include "Runtime/Script/CoroutineScheduler.h"
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

	void SetScriptName(const FString& InScriptName);
    bool RegisterScript();
    void UnregisterScript();

    bool LoadScript();
    bool HotReloadScript();
    void ClearScript();
    void StartCoroutine(sol::function Function);

	virtual void OnUnregister() override;

	virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual void EndPlay() override;

	const FString& GetScriptName() const { return ScriptName; }

	template <typename... Args>
    bool CallScriptFunction(const char* FunctionName, Args&&... args);

private:
    void ClearLoadedState();

	FString ScriptName;
    sol::environment ScriptEnv;
    FCoroutineScheduler CoroutineScheduler;

    bool bScriptRegistered = false;
    bool bScriptLoaded = false;
};

template <typename... Args>
bool UScriptComponent::CallScriptFunction(const char* FunctionName, Args&&... args)
{
    if (!bScriptLoaded || !ScriptEnv.valid())
    {
        return false;
    }

    sol::object Obj = ScriptEnv[FunctionName];

    if (!Obj.valid() || Obj == sol::nil)
    {
        return false;
    }

    if (Obj.get_type() != sol::type::function)
    {
        UE_LOG(
            "[Lua] '%s' exists but is not a function in script '%s'",
            FunctionName,
            ScriptName.c_str());
        return false;
    }

    sol::protected_function Func = Obj.as<sol::protected_function>();

    sol::protected_function_result Result =
        Func(std::forward<Args>(args)...);

    if (!Result.valid())
    {
        sol::error Err = Result;

        UE_LOG(
            "[Lua Runtime Error] Script: %s, Function: %s, Error: %s",
            ScriptName.c_str(),
            FunctionName,
            Err.what());

        return false;
    }

    return true;
}
