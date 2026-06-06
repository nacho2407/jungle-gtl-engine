#include "GameFramework/Gameplay/GameplayRuntime.h"

#include "Core/Logging/Log.h"
#include "Core/ProjectSettings.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Actor/PlayerStart.h"
#include "GameFramework/GameMode/LuaPlayerController.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/LuaCharacter.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Lua/LuaScriptManager.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"

FGameplayRuntime::~FGameplayRuntime()
{
	EndWorld();
}

void FGameplayRuntime::StartWorld(UWorld* InWorld)
{
	if (bStarted)
	{
		EndWorld();
	}

	World = InWorld;
	if (!World)
	{
		return;
	}

	const auto& Preset = FProjectSettings::Get().Game.GameplayPreset;
	DirectorModule = Preset.DirectorModule;

	// PlayerController는 world gameplay의 입력/카메라 owner이므로 가장 먼저 생성합니다.
	PlayerController = SpawnPlayerController();

	// 배치 Pawn 우선, 없으면 PlayerStart 기반 default Pawn 생성 순서입니다.
	if (Preset.bUsePlacedAutoPossessPawn)
	{
		PlayerPawn = FindPlacedAutoPossessPawn();
	}
	if (!PlayerPawn && Preset.bSpawnDefaultPawnIfMissing)
	{
		PlayerPawn = SpawnDefaultPawn();
	}

	PossessPawn(PlayerPawn);

	// Director는 PlayerController/Pawn 준비 뒤 Init, BeginPlay 순서로 호출합니다.
	if (LoadDirector())
	{
		CallDirectorWithContext("Init");
		CallDirectorWithContext("BeginPlay");
	}

	bStarted = true;
}

void FGameplayRuntime::Tick(float DeltaTime)
{
	if (!bStarted)
	{
		return;
	}

	CallDirectorTick(DeltaTime);
}

void FGameplayRuntime::EndWorld()
{
	if (!World && !bStarted)
	{
		ClearDirector();
		return;
	}

	// Director가 UI/EventBus를 정리할 기회를 먼저 제공합니다.
	CallDirectorNoArg("EndPlay");

	if (PlayerController)
	{
		PlayerController->UnPossess();
	}

	ClearDirector();
	PlayerPawn = nullptr;
	PlayerController = nullptr;
	World = nullptr;
	bStarted = false;
}

APlayerCameraManager* FGameplayRuntime::GetPlayerCameraManager() const
{
	return PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr;
}

UClass* FGameplayRuntime::ResolveClass(
	const FString& ClassName,
	UClass* RequiredBaseClass,
	UClass* FallbackClass,
	const char* SettingName) const
{
	if (!ClassName.empty())
	{
		UClass* FoundClass = UClass::FindByName(ClassName.c_str());
		if (FoundClass && (!RequiredBaseClass || FoundClass->IsA(RequiredBaseClass)))
		{
			return FoundClass;
		}

		UE_LOG("[GameplayRuntime] %s '%s' not found or invalid. Fallback to %s.",
			SettingName,
			ClassName.c_str(),
			FallbackClass ? FallbackClass->GetName() : "(null)");
	}

	return FallbackClass;
}

APlayerController* FGameplayRuntime::SpawnPlayerController()
{
	if (!World)
	{
		return nullptr;
	}

	const auto& Preset = FProjectSettings::Get().Game.GameplayPreset;
	UClass* ControllerClass = ResolveClass(
		Preset.PlayerControllerClassName,
		APlayerController::StaticClass(),
		ALuaPlayerController::StaticClass(),
		"PlayerControllerClassName");

	AActor* SpawnedActor = World->SpawnActorByClass(ControllerClass);
	APlayerController* Controller = Cast<APlayerController>(SpawnedActor);
	if (!Controller)
	{
		UE_LOG("[GameplayRuntime] Failed to spawn PlayerController.");
	}
	return Controller;
}

APawn* FGameplayRuntime::FindPlacedAutoPossessPawn() const
{
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && Pawn->GetAutoPossessPlayer())
		{
			UE_LOG("[GameplayRuntime] Use placed auto-possess Pawn: %s", Pawn->GetName().c_str());
			return Pawn;
		}
	}

	return nullptr;
}

APawn* FGameplayRuntime::SpawnDefaultPawn()
{
	if (!World)
	{
		return nullptr;
	}

	const auto& Preset = FProjectSettings::Get().Game.GameplayPreset;
	UClass* PawnClass = ResolveClass(
		Preset.DefaultPawnClassName,
		APawn::StaticClass(),
		ALuaCharacter::StaticClass(),
		"DefaultPawnClassName");
	if (!PawnClass)
	{
		return nullptr;
	}

	// BeginPlay가 이미 시작된 world에서도 컴포넌트 초기화 후 actor BeginPlay가 실행되게 직접 생성합니다.
	UObject* CreatedObject = FObjectFactory::Get().Create(PawnClass->GetName(), World);
	APawn* Pawn = Cast<APawn>(CreatedObject);
	if (!Pawn)
	{
		if (CreatedObject)
		{
			UObjectManager::Get().DestroyObject(CreatedObject);
		}
		UE_LOG("[GameplayRuntime] Failed to create default Pawn class: %s", PawnClass->GetName());
		return nullptr;
	}

	if (ALuaCharacter* LuaCharacter = Cast<ALuaCharacter>(Pawn))
	{
		LuaCharacter->InitDefaultComponents(Preset.DefaultPawnMeshPath, Preset.DefaultPawnScript);
	}
	else if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->InitDefaultComponents(Preset.DefaultPawnMeshPath);
	}

	if (AActor* Start = FindPlayerStart())
	{
		Pawn->SetActorLocation(Start->GetActorLocation());
		Pawn->SetActorRotation(Start->GetActorRotation());
	}

	Pawn->SetAutoPossessPlayer(true);
	World->AddActor(Pawn);
	UE_LOG("[GameplayRuntime] Spawned default Pawn: %s", Pawn->GetName().c_str());
	return Pawn;
}

AActor* FGameplayRuntime::FindPlayerStart() const
{
	if (!World)
	{
		return nullptr;
	}

	const FString& RequiredTag = FProjectSettings::Get().Game.GameplayPreset.DefaultPlayerStartTag;
	APlayerStart* FirstStart = nullptr;
	for (AActor* Actor : World->GetActors())
	{
		APlayerStart* Start = Cast<APlayerStart>(Actor);
		if (!Start)
		{
			continue;
		}

		if (!FirstStart)
		{
			FirstStart = Start;
		}

		if (RequiredTag.empty() || Start->GetStartTag().ToString() == RequiredTag)
		{
			return Start;
		}
	}

	return FirstStart;
}

void FGameplayRuntime::PossessPawn(APawn* Pawn)
{
	if (!PlayerController || !Pawn)
	{
		if (!Pawn)
		{
			UE_LOG("[GameplayRuntime] No Pawn to possess.");
		}
		return;
	}

	PlayerController->Possess(Pawn);
}

bool FGameplayRuntime::LoadDirector()
{
	ClearDirector();
	if (DirectorModule.empty())
	{
		return false;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::protected_function Require = Lua["require"];
	if (!Require.valid())
	{
		return false;
	}

	sol::protected_function_result Result = Require(DirectorModule);
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[GameplayRuntime] Director require failed: %s", Error.what());
		return false;
	}

	sol::object ModuleObject = Result.get<sol::object>();
	if (!ModuleObject.valid() || ModuleObject.get_type() != sol::type::table)
	{
		UE_LOG("[GameplayRuntime] Director module '%s' did not return a table.", DirectorModule.c_str());
		return false;
	}

	DirectorTable = ModuleObject.as<sol::table>();
	return true;
}

void FGameplayRuntime::CallDirectorNoArg(const char* FunctionName)
{
	if (!DirectorTable.valid())
	{
		return;
	}

	sol::object FunctionObject = DirectorTable[FunctionName];
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result = Function();
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[GameplayRuntime] Director.%s error: %s", FunctionName, Error.what());
	}
}

void FGameplayRuntime::CallDirectorWithContext(const char* FunctionName)
{
	if (!DirectorTable.valid())
	{
		return;
	}

	sol::object FunctionObject = DirectorTable[FunctionName];
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result = Function(MakeDirectorContext());
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[GameplayRuntime] Director.%s error: %s", FunctionName, Error.what());
	}
}

void FGameplayRuntime::CallDirectorTick(float DeltaTime)
{
	if (!DirectorTable.valid())
	{
		return;
	}

	sol::object FunctionObject = DirectorTable["Tick"];
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::protected_function_result Result = Function(DeltaTime);
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[GameplayRuntime] Director.Tick error: %s", Error.what());
	}
}

sol::table FGameplayRuntime::MakeDirectorContext() const
{
	sol::state& Lua = FLuaScriptManager::GetState();
	sol::table Context = Lua.create_table();
	Context["PlayerController"] = PlayerController;
	Context["PlayerPawn"] = PlayerPawn;
	Context["DirectorModule"] = DirectorModule;
	return Context;
}

void FGameplayRuntime::ClearDirector()
{
	DirectorTable = sol::table();
}
