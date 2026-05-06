#pragma once
#include "Animation/TimelinePlayer.h"
#include "Component/ActorComponent.h"
#include "Core/Logging/Log.h"
#include "Runtime/Script/CoroutineScheduler.h"
#include "ThirdParty/sol/sol.hpp"
#include <filesystem>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class UPrimitiveComponent;
class UCurveFloatAsset;
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

class FLuaTimeline
{
public:
    void Play();
    void Pause();
    void Stop();
    void Tick(float DeltaTime);
    void SetPlayRate(float InPlayRate);
    void SetLoop(bool bInLoop);
    float GetCurrentTime() const;
    void SetCurrentTime(float InCurrentTime);
    void AddFloatTrack(const FString& TrackName, const sol::table& PlaybackDesc, sol::function OnUpdate);
    void ClearTracks();

private:
    FCurvePlaybackDesc MakePlaybackDesc(const sol::table& PlaybackDesc) const;

private:
    FTimelinePlayer Player;
};

class UScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UScriptComponent, UActorComponent)
	UScriptComponent() = default;
	~UScriptComponent() override;

	void PostDuplicate(UObject* Original) override;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetScriptName(const FString& InScriptName);
    bool RegisterScript();
    void UnregisterScript();

    bool LoadScript();
    bool HotReloadScript();
    void ClearScript();
    void ReleaseLuaStateReferences();
    void StartCoroutine(sol::function Function);
    FLuaTimeline* CreateTimeline();
    void ClearLuaTimelines();

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
    bool ApplyLuaPropertyToInstance(const char* PropertyName);
    FString ScriptName;
    FString RegisteredScriptName;

    sol::environment ScriptEnv;

    sol::table ScriptClass;
    sol::table ScriptInstance;
    FCoroutineScheduler CoroutineScheduler;
    TArray<FLuaTimeline*> LuaTimelines;

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
        UE_LOG_ERROR("[ScriptComponent] Lua Error in %s: %s", FunctionName.c_str(), Err.what());
    }
}
