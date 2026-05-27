#include "LuaBlueprintComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "Lua/LuaScriptManager.h"
#include "LuaBlueprint/LuaBlueprintAsset.h"
#include "LuaBlueprint/LuaBlueprintManager.h"
#include "Object/GarbageCollection.h"

#include <cstring>

ULuaBlueprintComponent::ULuaBlueprintComponent() = default;
ULuaBlueprintComponent::~ULuaBlueprintComponent() = default;

void ULuaBlueprintComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UActorComponent::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BlueprintAsset, "ULuaBlueprintComponent::BlueprintAsset");
}

void ULuaBlueprintComponent::BeginDestroy()
{
	ClearCollisionBindings();
	ClearLuaRuntime();
	UActorComponent::BeginDestroy();
}

void ULuaBlueprintComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	ReloadBlueprint();
	if (LuaBeginPlay)
	{
		sol::protected_function_result Result = LuaBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("LuaBlueprint BeginPlay error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
	BindOwnerCollisionEvents();
}

void ULuaBlueprintComponent::EndPlay()
{
	UActorComponent::EndPlay();
	ClearCollisionBindings();
	if (LuaEndPlay)
	{
		sol::protected_function_result Result = LuaEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("LuaBlueprint EndPlay error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
	ClearLuaRuntime();
}

void ULuaBlueprintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (BlueprintAsset && LoadedBlueprintVersion != BlueprintAsset->GetVersion())
	{
		ReloadBlueprint();
	}

	if (LuaTick)
	{
		sol::protected_function_result Result = LuaTick(DeltaTime);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("LuaBlueprint Tick error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
}

bool ULuaBlueprintComponent::LoadBlueprintAsset()
{
	if (BlueprintAsset && LoadedBlueprintVersion == BlueprintAsset->GetVersion())
	{
		return true;
	}

	if (!BlueprintPath.empty() && BlueprintPath != "None")
	{
		BlueprintAsset = FLuaBlueprintManager::Get().Load(BlueprintPath);
	}

	if (!BlueprintAsset)
	{
		return false;
	}

	if (BlueprintAsset->IsCompileDirty())
	{
		BlueprintAsset->Compile();
	}

	if (BlueprintAsset->HasCompileErrors())
	{
		UE_LOG("LuaBlueprint load failed because compile errors exist: %s", BlueprintPath.c_str());
		return false;
	}

	LoadedBlueprintVersion = BlueprintAsset->GetVersion();
	return true;
}

bool ULuaBlueprintComponent::ReloadBlueprint()
{
	ClearCollisionBindings();
	const bool bInitialized = InitializeLua();
	if (bInitialized)
	{
		BindOwnerCollisionEvents();
	}
	return bInitialized;
}

bool ULuaBlueprintComponent::InitializeLua()
{
	ClearLuaRuntime();

	if (!LoadBlueprintAsset())
	{
		return false;
	}

	const FString& Source = BlueprintAsset->GetGeneratedLuaSource();
	if (Source.empty())
	{
		UE_LOG("LuaBlueprint has empty generated source: %s", BlueprintPath.c_str());
		return false;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	Env = sol::environment(Lua, sol::create, Lua.globals());
	Env["obj"] = GetOwner();
	Env["this"] = this;
	Env["component"] = this;

	const FString ChunkName = BlueprintPath.empty() ? GetRuntimeName() : BlueprintPath;
	sol::protected_function_result Result = Lua.safe_script(Source, Env, sol::script_pass_on_error, ChunkName);
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Failed to load LuaBlueprint %s: %s", ChunkName.c_str(), Err.what());
		ClearLuaRuntime();
		return false;
	}

	LuaBeginPlay = Env["BeginPlay"];
	LuaTick = Env["Tick"];
	LuaEndPlay = Env["EndPlay"];
	LuaOnOverlap = Env["OnOverlap"];
	LuaOnEndOverlap = Env["OnEndOverlap"];
	LuaOnHit = Env["OnHit"];
	LuaOnEndHit = Env["OnEndHit"];
	LoadedBlueprintVersion = BlueprintAsset->GetVersion();
	return true;
}

void ULuaBlueprintComponent::ClearLuaRuntime()
{
	LuaBeginPlay = sol::nil;
	LuaTick = sol::nil;
	LuaEndPlay = sol::nil;
	LuaOnOverlap = sol::nil;
	LuaOnEndOverlap = sol::nil;
	LuaOnHit = sol::nil;
	LuaOnEndHit = sol::nil;
	Env = sol::environment();
}

void ULuaBlueprintComponent::BindOwnerCollisionEvents()
{
	ClearCollisionBindings();

	if (!LuaOnOverlap && !LuaOnEndOverlap && !LuaOnHit && !LuaOnEndHit)
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
			BeginOverlapHandles.push_back(LuaOnOverlap
				? PrimitiveComponent->OnComponentBeginOverlap.AddRaw(this, &ULuaBlueprintComponent::HandleBeginOverlap)
				: FDelegateHandle());
			EndOverlapHandles.push_back(LuaOnEndOverlap
				? PrimitiveComponent->OnComponentEndOverlap.AddRaw(this, &ULuaBlueprintComponent::HandleEndOverlap)
				: FDelegateHandle());
		}

		if (LuaOnHit || LuaOnEndHit)
		{
			BoundHitComponents.push_back(PrimitiveComponent);
			HitHandles.push_back(LuaOnHit
				? PrimitiveComponent->OnComponentHit.AddRaw(this, &ULuaBlueprintComponent::HandleHit)
				: FDelegateHandle());
			EndHitHandles.push_back(LuaOnEndHit
				? PrimitiveComponent->OnComponentEndHit.AddRaw(this, &ULuaBlueprintComponent::HandleEndHit)
				: FDelegateHandle());
		}
	}
}

void ULuaBlueprintComponent::ClearCollisionBindings()
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
		if (Index < static_cast<int32>(EndHitHandles.size()) && EndHitHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentEndHit.Remove(EndHitHandles[Index]);
		}
	}
	BoundHitComponents.clear();
	HitHandles.clear();
	EndHitHandles.clear();
}

void ULuaBlueprintComponent::HandleBeginOverlap(
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
			UE_LOG("LuaBlueprint OnOverlap error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
}

void ULuaBlueprintComponent::HandleEndOverlap(
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
			UE_LOG("LuaBlueprint OnEndOverlap error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
}

void ULuaBlueprintComponent::HandleHit(
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
			UE_LOG("LuaBlueprint OnHit error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
}

void ULuaBlueprintComponent::HandleEndHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp)
{
	if (LuaOnEndHit)
	{
		sol::protected_function_result Result = LuaOnEndHit(OtherActor, HitComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("LuaBlueprint OnEndHit error in %s: %s", GetRuntimeName().c_str(), Err.what());
		}
	}
}

bool ULuaBlueprintComponent::CallFunction(const FString& FunctionName)
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
		UE_LOG("LuaBlueprint %s error in %s: %s", FunctionName.c_str(), GetRuntimeName().c_str(), Err.what());
		return false;
	}
	return true;
}

void ULuaBlueprintComponent::PreGetEditableProperties()
{
	UActorComponent::PreGetEditableProperties();
	LoadBlueprintAsset();
}

void ULuaBlueprintComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (PropertyName && std::strcmp(PropertyName, "BlueprintPath") == 0)
	{
		BlueprintAsset = nullptr;
		LoadedBlueprintVersion = 0;
		ReloadBlueprint();
	}
}

FString ULuaBlueprintComponent::GetRuntimeName() const
{
	if (!BlueprintPath.empty())
	{
		return BlueprintPath;
	}
	if (BlueprintAsset && !BlueprintAsset->GetSourcePath().empty())
	{
		return BlueprintAsset->GetSourcePath();
	}
	return "TransientLuaBlueprint";
}
