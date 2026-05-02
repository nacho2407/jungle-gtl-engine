#pragma once
#include "Component/ActorComponent.h"
#include "Core/Logging/Log.h"
#include "Runtime/Script/CoroutineScheduler.h"
#include "ThirdParty/sol/sol.hpp"

class UPrimitiveComponent;
struct FHitResult;
struct FLuaScriptLoadResult;

enum class ELuaScriptPropertyType
{
    Int,
    Float,
    Bool,
    String,
	Vector,
};

struct FLuaScriptProperty
{
    FString Name;
    FString TypeName;
    EPropertyType Type;

    int IntValue = 0;
    float FloatValue = 0.0f;
    bool BoolValue = false;
    FString StringValue;
    FVector Vec3Value;

    FString Category;

    bool bHasMin = false;
    bool bHasMax = false;
    float Min = 0.0f;
    float Max = 0.0f;
};

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

	//LuaScript Class의 인스턴스를 반환
    sol::optional<sol::table> CreateScriptInstance(const FLuaScriptLoadResult& Loaded);        
    void ReloadLuaProperties();
	sol::table MakeRuntimePropertyTable(sol::state& Lua);
	
	template <typename... Args>
    void CallScriptFunction(const FString& FunctionName, Args&&... args);

	void OnHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

	void OnBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

	void OnEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);


private:
    TArray<FLuaScriptProperty> LuaProperties;
    bool bLuaPropertiesScanned = false;

private:
    void ClearLoadedState();
	FString ScriptName;
    sol::environment ScriptEnv;

    sol::table ScriptClass;
    sol::table ScriptInstance;
    FCoroutineScheduler CoroutineScheduler;

    bool bScriptRegistered = false;
    bool bScriptLoaded = false;
};

template <typename... Args>
void UScriptComponent::CallScriptFunction(const FString& FunctionName, Args&&... args)
{
    if (!ScriptInstance.valid())
    {
        return;
    }

    sol::object FuncObj = ScriptInstance[FunctionName];

    if (!FuncObj.valid() || FuncObj.get_type() != sol::type::function)
    {
        return;
    }

    sol::protected_function Func = FuncObj.as<sol::protected_function>();

    sol::protected_function_result Result =
        Func(ScriptInstance, std::forward<Args>(args)...);

    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG("[ScriptComponent] Lua Error in %s: %s", FunctionName.c_str(), Err.what());
    }
}