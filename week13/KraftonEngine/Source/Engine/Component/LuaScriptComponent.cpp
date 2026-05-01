#include "LuaScriptComponent.h"

#include "Core/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Lua/LuaScriptManager.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULuaScriptComponent, UActorComponent)

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
}

void ULuaScriptComponent::InitializeLua()
{
	sol::state& Lua = FLuaScriptManager::GetState();

	Env = sol::environment(Lua, sol::create, Lua.globals());
	Env["obj"] = GetOwner();
	Env["this"] = this;

	const FString ResolvedPath = FLuaScriptManager::ResolveScriptPath(ScriptFile);
	sol::protected_function_result Result = Lua.safe_script_file(ResolvedPath.c_str(), Env, sol::script_pass_on_error);

	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Failed to load Lua script %s: %s", ScriptFile.c_str(), Err.what());
		return;
	}

	LuaBeginPlay = Env["BeginPlay"];
	LuaTick = Env["Tick"];
	LuaEndPlay = Env["EndPlay"];
}

void ULuaScriptComponent::BeginPlay()
{
	EnsureDefaultScriptFile();
	UActorComponent::BeginPlay();

	InitializeLua();

	if (LuaBeginPlay)
	{
		sol::protected_function_result Result = LuaBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua BeginPlay error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::EndPlay()
{
	UActorComponent::EndPlay();
	if (LuaEndPlay)
	{
		sol::protected_function_result Result = LuaEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua EndPlay error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (LuaTick)
	{
		sol::protected_function_result Result = LuaTick(DeltaTime);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua Tick error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureDefaultScriptFile();
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "ScriptFile", EPropertyType::Script, &ScriptFile });
}

void ULuaScriptComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << ScriptFile;
}

void ULuaScriptComponent::EnsureDefaultScriptFile()
{
	if (!ScriptFile.empty())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->GetFName().IsValid())
	{
		return;
	}

	ULevel* Level = OwnerActor->GetLevel();
	if (!Level || !Level->GetFName().IsValid())
	{
		return;
	}

	ScriptFile = Level->GetFName().ToString() + "_" + OwnerActor->GetFName().ToString() + ".lua";
}
