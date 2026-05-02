#include "ScriptComponent.h"
#include "ScriptManager.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include <Core/CollisionTypes.h>

#include "Core/Paths.h"
#include "Core/ResourceManager.h"

DEFINE_CLASS(UScriptComponent, UActorComponent)
REGISTER_FACTORY(UScriptComponent)

namespace
{
    void BindCoroutineHelpers(sol::environment& Env, UScriptComponent* ScriptComponent)
    {
        if (!Env.valid() || !ScriptComponent)
        {
            return;
        }

        Env["StartCoroutine"] = [ScriptComponent](sol::function Function)
        {
            ScriptComponent->StartCoroutine(Function);
        };

        Env["WaitForSeconds"] = [](sol::this_state State, float Seconds)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "seconds";
            Table["value"] = Seconds;
            return Table;
        };

        Env["WaitForFrames"] = [](sol::this_state State, int Frames)
        {
            sol::state_view Lua(State);
            sol::table Table = Lua.create_table();
            Table["type"] = "frames";
            Table["value"] = Frames;
            return Table;
        };
    }
}

void UScriptComponent::PostDuplicate(UObject* Original)
{
    UActorComponent::PostDuplicate(Original);
}

void UScriptComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << "ScriptName" << ScriptName;
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "ScriptName", EPropertyType::String, &ScriptName });
}

void UScriptComponent::SetScriptName(const FString& InScriptName)
{
    if (ScriptName == InScriptName)
    {
        return;
    }

    UnregisterScript();

    ScriptName = InScriptName;
    ClearLoadedState();
}

bool UScriptComponent::RegisterScript()
{
    if (ScriptName.empty())
    {
        return false;
    }

    if (bScriptRegistered)
    {
        return true;
    }

    if (!FScriptManager::Get().HasScript(ScriptName))
    {
        UE_LOG("[ScriptComponent] Script not found: %s", ScriptName.c_str());
        return false;
    }

    FScriptManager::Get().RegisterScriptComponents(ScriptName, this);
    bScriptRegistered = true;

    return true;
}

void UScriptComponent::UnregisterScript()
{
    if (!bScriptRegistered)
    {
        return;
    }

    FScriptManager::Get().UnregisterScriptComponents(ScriptName, this);
    bScriptRegistered = false;
}

void UScriptComponent::ClearScript()
{
    UnregisterScript();

    ScriptName.clear();
    ClearLoadedState();
}

bool UScriptComponent::LoadScript()
{
    if (ScriptName.empty())
    {
        ClearLoadedState();
        return false;
    }

    auto LoadedEnv = FScriptManager::Get().LoadLuaEnvironment(this, ScriptName);

    if (LoadedEnv == std::nullopt)
    {
        ClearLoadedState();
        return false;
    }

    sol::environment NewEnv = std::move(*LoadedEnv);

    CoroutineScheduler.StopAll();
    ScriptEnv = std::move(NewEnv);
    BindCoroutineHelpers(ScriptEnv, this);
    bScriptLoaded = true;

    RegisterScript();

    return true;
}

bool UScriptComponent::HotReloadScript()
{
    if (ScriptName.empty())
    {
        return false;
    }

    auto LoadedEnv = FScriptManager::Get().LoadLuaEnvironment(this, ScriptName);

    if (LoadedEnv == std::nullopt)
    {
        UE_LOG("[ScriptComponent] Reload failed. Keeping previous script: %s", ScriptName.c_str());
        return false;
    }

    sol::environment NewEnv = std::move(*LoadedEnv);

    CoroutineScheduler.StopAll();
    ScriptEnv = std::move(NewEnv);
    BindCoroutineHelpers(ScriptEnv, this);
    bScriptLoaded = true;

    return true;
}

void UScriptComponent::StartCoroutine(sol::function Function)
{
    if (!Function.valid())
    {
        UE_LOG("[ScriptComponent] Invalid coroutine function in script '%s'", ScriptName.c_str());
        return;
    }

    CoroutineScheduler.StartCoroutine(Function);
}

void UScriptComponent::OnUnregister()
{ 
	// 부모 훅 호출
    UActorComponent::OnUnregister();

    // 스크립트가 등록되어있다면 매니저에 언레지스터 요청
    UnregisterScript();
}

// Lifecycle 훅에서 Lua 함수 호출. Lua 환경이 유효한 경우에만 시도합니다.
void UScriptComponent::BeginPlay()
{
    UActorComponent::BeginPlay();

	for (auto Comp : Owner->GetComponents())
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
		{
            OnComponentBeginOverlapHandleId = Prim->OnComponentBeginOverlap.AddDynamic(
				this,
				&UScriptComponent::OnBeginOverlap);
			
			OnComponentEndOverlapHandleId = Prim->OnComponentEndOverlap.AddDynamic(
				this,
				&UScriptComponent::OnEndOverlap);

			OnComponentHitHandleId = Prim->OnComponentHit.AddDynamic(
				this,
				&UScriptComponent::OnHit);
		}
    }

    if (!bScriptLoaded)
    {
        LoadScript();
    }
	CallScriptFunction("BeginPlay");
}

void UScriptComponent::TickComponent(float DeltaTime)
{
    UActorComponent::TickComponent(DeltaTime);

    if (!bScriptLoaded)
    {
        return;
    }

    CallScriptFunction("Tick", DeltaTime);
    CoroutineScheduler.Tick(DeltaTime);
}

void UScriptComponent::EndPlay()
{
    UActorComponent::EndPlay();

	for (auto Comp : Owner->GetComponents())
    {
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
        {
            Prim->OnComponentBeginOverlap.Remove(OnComponentBeginOverlapHandleId);
            Prim->OnComponentEndOverlap.Remove(OnComponentEndOverlapHandleId);
            Prim->OnComponentHit.Remove(OnComponentHitHandleId);
        }
    }


	if (!ScriptEnv.valid())
	{
        CoroutineScheduler.StopAll();
		return;
	}

    CallScriptFunction("EndPlay");
    CoroutineScheduler.StopAll();
}

void UScriptComponent::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	CallScriptFunction("OnHit", HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
}

void UScriptComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    CallScriptFunction("OnBeginOverlap", OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void UScriptComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    CallScriptFunction("OnEndOverlap", OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void UScriptComponent::ClearLoadedState()
{
    bScriptLoaded = false;
    CoroutineScheduler.StopAll();

    // 굳이 ScriptEnv.clear() 하지 마라.
    // invalid env에서 clear를 부르면 다시 사고 날 수 있다.
    ScriptEnv = sol::environment{};
}
