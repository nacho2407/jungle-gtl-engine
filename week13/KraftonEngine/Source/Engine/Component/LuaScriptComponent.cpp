#include "LuaScriptComponent.h"

#include "Component/PrimitiveComponent.h"
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
	LuaBeginPlay = sol::nil;
	LuaTick = sol::nil;
	LuaEndPlay = sol::nil;
	LuaOnOverlap = sol::nil;
	LuaOnEndOverlap = sol::nil;
	LuaOnHit = sol::nil;

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
	LuaOnOverlap = Env["OnOverlap"];
	LuaOnEndOverlap = Env["OnEndOverlap"];
	LuaOnHit = Env["OnHit"];
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

	BindOwnerCollisionEvents();
}

void ULuaScriptComponent::EndPlay()
{
	UActorComponent::EndPlay();
	ClearCollisionBindings();
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

void ULuaScriptComponent::BindOwnerCollisionEvents()
{
	ClearCollisionBindings();

	if (!LuaOnOverlap && !LuaOnEndOverlap && !LuaOnHit)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	for (UPrimitiveComponent* PrimitiveComponent : OwnerActor->GetPrimitiveComponents())
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if ((LuaOnOverlap || LuaOnEndOverlap) && PrimitiveComponent->GetGenerateOverlapEvents())
		{
			BoundOverlapComponents.push_back(PrimitiveComponent);
			BeginOverlapHandles.push_back(PrimitiveComponent->OnComponentBeginOverlap.AddRaw(this, &ULuaScriptComponent::HandleBeginOverlap));
			EndOverlapHandles.push_back(PrimitiveComponent->OnComponentEndOverlap.AddRaw(this, &ULuaScriptComponent::HandleEndOverlap));
		}

		if (LuaOnHit)
		{
			BoundHitComponents.push_back(PrimitiveComponent);
			HitHandles.push_back(PrimitiveComponent->OnComponentHit.AddRaw(this, &ULuaScriptComponent::HandleHit));
		}
	}
}

void ULuaScriptComponent::ClearCollisionBindings()
{
	for (int32 Index = 0; Index < static_cast<int32>(BoundOverlapComponents.size()); ++Index)
	{
		UPrimitiveComponent* PrimitiveComponent = BoundOverlapComponents[Index];
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (Index < static_cast<int32>(BeginOverlapHandles.size()) && BeginOverlapHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentBeginOverlap.Remove(BeginOverlapHandles[Index]);
		}

		if (Index < static_cast<int32>(EndOverlapHandles.size()) && EndOverlapHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentEndOverlap.Remove(EndOverlapHandles[Index]);
		}
	}

	BoundOverlapComponents.clear();
	BeginOverlapHandles.clear();
	EndOverlapHandles.clear();

	for (int32 Index = 0; Index < static_cast<int32>(BoundHitComponents.size()); ++Index)
	{
		UPrimitiveComponent* PrimitiveComponent = BoundHitComponents[Index];
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (Index < static_cast<int32>(HitHandles.size()) && HitHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentHit.Remove(HitHandles[Index]);
		}
	}

	BoundHitComponents.clear();
	HitHandles.clear();
}

void ULuaScriptComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (LuaOnOverlap)
	{
		sol::protected_function_result Result = LuaOnOverlap(OtherActor, OverlappedComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/)
{
	if (LuaOnEndOverlap)
	{
		sol::protected_function_result Result = LuaOnEndOverlap(OtherActor, OverlappedComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnEndOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& HitResult)
{
	if (LuaOnHit)
	{
		sol::protected_function_result Result = LuaOnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnHit error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

bool ULuaScriptComponent::CallFunction(const FString& FunctionName)
{
	if (!Env.valid())
	{
		return false;
	}

	sol::object Target = Env[FunctionName.c_str()];
	if (!Target.valid() || Target.get_type() != sol::type::function)
	{
		return false;
	}

	sol::protected_function Fn = Target;
	sol::protected_function_result Result = Fn();
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Lua %s error in %s: %s", FunctionName.c_str(), ScriptFile.c_str(), Err.what());
		return false;
	}
	return true;
}

void ULuaScriptComponent::DispatchOverlap(AActor* OtherActor)
{
	if (LuaOnOverlap)
	{
		sol::protected_function_result Result = LuaOnOverlap(OtherActor);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
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
	OutProps.push_back({ "ScriptFile", EPropertyType::Script, "Script", &ScriptFile });
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
