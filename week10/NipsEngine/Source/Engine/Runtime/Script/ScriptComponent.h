#pragma once
#include "Component/ActorComponent.h"
#include "Core/Logging/Log.h"
#include "Runtime/Script/CoroutineScheduler.h"
#include "ThirdParty/sol/sol.hpp"

class UPrimitiveComponent;
struct FHitResult;

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

	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	template <typename... Args>
    bool CallScriptFunction(const FString& FunctionName, Args&&... args);

private:
    void ClearLoadedState();
	
    uint64 OnComponentBeginOverlapHandleId = 0;
    uint64 OnComponentEndOverlapHandleId = 0;
    uint64 OnComponentHitHandleId = 0;

	FString ScriptName;
    sol::environment ScriptEnv;
    FCoroutineScheduler CoroutineScheduler;

    bool bScriptRegistered = false;
    bool bScriptLoaded = false;
};

template <typename... Args>
bool UScriptComponent::CallScriptFunction(const FString& FunctionName, Args&&... args)
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
