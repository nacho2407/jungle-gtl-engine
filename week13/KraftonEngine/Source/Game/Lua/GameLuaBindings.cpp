#include "Game/Lua/GameLuaBindings.h"

#include "sol/sol.hpp"

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "Lua/LuaScriptManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Object/Object.h"  // Cast
#include "Core/Log.h"

#include "Game/Component/Movement/CarMovementComponent.h"
#include "Game/Component/CarGasComponent.h"
#include "Game/Component/DirtComponent.h"

#include "Game/GameEngine.h"
#include "Game/GameMode/GameModeCarGame.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Game/Pawn/CarPawn.h"
#include "Game/Pawn/PoliceCar.h"

void RegisterGameLuaBindings(sol::state& Lua)
{
	// --- 차량 컴포넌트 usertype ---
	Lua.new_usertype<UCarMovementComponent>("CarMovementComponent",
		"SetThrottleInput", &UCarMovementComponent::SetThrottleInput,
		"SetSteeringInput", &UCarMovementComponent::SetSteeringInput,
		"StopImmediately",  &UCarMovementComponent::StopImmediately,
		"GetForwardSpeed",  &UCarMovementComponent::GetForwardSpeed);

	Lua.new_usertype<UCarGasComponent>("CarGasComponent",
		"SetGas",      &UCarGasComponent::SetGas,
		"AddGas",      &UCarGasComponent::AddGas,
		"ConsumeGas",  &UCarGasComponent::ConsumeGas,
		"GetGas",      &UCarGasComponent::GetGas,
		"GetMaxGas",   &UCarGasComponent::GetMaxGas,
		"GetGasRatio", &UCarGasComponent::GetGasRatio,
		"HasGas",      &UCarGasComponent::HasGas);

	// --- AActor 확장 메서드 ---
	// Engine 측이 이미 "Actor" usertype 을 등록한 상태. 같은 usertype 의 추가 메서드를
	// 같은 키의 sol::usertype 핸들로 받아 set 하면 sol2 가 metatable 에 추가해준다.
	// Lua 스크립트에서 obj:AsCarPawn() / obj:GetCarMovement() 등이 그대로 동작.
	sol::usertype<AActor> ActorType = Lua["Actor"];
	ActorType["GetCarMovement"] = [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UCarMovementComponent>();
	};
	ActorType["GetCarGas"] = [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UCarGasComponent>();
	};
	ActorType["AsCarPawn"] = [](AActor& Actor)
	{
		return Cast<ACarPawn>(&Actor);
	};
	ActorType["AsPoliceCar"] = [](AActor& Actor)
	{
		return Cast<APoliceCar>(&Actor);
	};
	ActorType["FireCarWashRay"] = [](AActor& Actor)
	{
		return UDirtComponent::FireCarWashRay(Actor);
	};
	ActorType["SetCarWashStreamVisible"] = [](AActor& Actor, bool bVisible)
	{
		UDirtComponent::SetCarWashStreamVisible(Actor, bVisible);
	};
	ActorType["IsCarWashStreamVisible"] = [](AActor& Actor)
	{
		return UDirtComponent::IsCarWashStreamVisible(Actor);
	};
	ActorType["AreAllDirtComponentsWashed"] = [](AActor& Actor)
	{
		return UDirtComponent::AreAllDirtComponentsWashed(Actor);
	};
	ActorType["SetVisible"] = [](AActor& Actor, bool bVisible)
	{
		Actor.SetVisible(bVisible);
	};
	ActorType["IsVisible"] = [](AActor& Actor)
	{
		return Actor.IsVisible();
	};

	// --- ACarPawn / APoliceCar usertype ---
	Lua.new_usertype<ACarPawn>("CarPawn",
		sol::base_classes, sol::bases<APawn, AActor>(),
		"GetCarMovement", [](ACarPawn& Pawn)
	{
		return Pawn.GetComponentByClass<UCarMovementComponent>();
	},
		"GetCarGas",          &ACarPawn::GetGas,
		"GetGas",             &ACarPawn::GetGas,
		"TakeDamage",         &ACarPawn::TakeDamage,
		"GetHealth",          &ACarPawn::GetHealth,
		"IsFirstPersonView",  &ACarPawn::IsFirstPersonView);

	Lua.new_usertype<APoliceCar>("PoliceCar",
		sol::base_classes, sol::bases<ACarPawn, APawn, AActor>(),
		"GetTarget", &APoliceCar::GetTarget);

	// --- 페이즈 enum + GameState ---
	Lua.new_enum("ECarGamePhase",
		"None",         ECarGamePhase::None,
		"CarWash",      ECarGamePhase::CarWash,
		"CarGas",       ECarGamePhase::CarGas,
		"EscapePolice", ECarGamePhase::EscapePolice,
		"DodgeMeteor",  ECarGamePhase::DodgeMeteor,
		"Result",       ECarGamePhase::Result,
		"Finished",     ECarGamePhase::Finished);

	Lua.new_enum("EPhaseResult",
		"None",    EPhaseResult::None,
		"Success", EPhaseResult::Success,
		"Failed",  EPhaseResult::Failed);

	Lua.new_usertype<AGameModeCarGame>("GameModeCarGame",
		"SuccessPhase", &AGameModeCarGame::SuccessPhase);

	Lua.new_usertype<AGameStateCarGame>("GameStateCarGame",
		"GetPhase",              &AGameStateCarGame::GetPhase,
		"SetPhase",              &AGameStateCarGame::SetPhase,
		"GetQuestPhase",         &AGameStateCarGame::GetQuestPhase,
		"SetQuestPhase",         &AGameStateCarGame::SetQuestPhase,
		"GetRemainingMatchTime", &AGameStateCarGame::GetRemainingMatchTime,
		"GetRemainingPhaseTime", &AGameStateCarGame::GetRemainingPhaseTime,
		"SetRemainingPhaseTime", &AGameStateCarGame::SetRemainingPhaseTime,
		"GetLastEndedPhase",     &AGameStateCarGame::GetLastEndedPhase,
		"GetLastPhaseResult",    &AGameStateCarGame::GetLastPhaseResult,
		"GetClearedPhasesMask",  &AGameStateCarGame::GetClearedPhasesMask,
		"BindPhaseChanged", [](AGameStateCarGame& GameState, sol::protected_function Callback)
	{
		GameState.OnPhaseChanged.AddLambda([Callback](ECarGamePhase NewPhase) mutable
		{
			if (!Callback.valid()) return;
			sol::protected_function_result Result = Callback(NewPhase);
			if (!Result.valid())
			{
				sol::error Err = Result;
				UE_LOG("[Lua] Phase changed callback error: %s", Err.what());
			}
		});
	});

	Lua["GetGameMode"] = []() -> AGameModeCarGame*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		return W ? Cast<AGameModeCarGame>(W->GetGameMode()) : nullptr;
	};

	Lua["GetGameState"] = []() -> AGameStateCarGame*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		return W ? Cast<AGameStateCarGame>(W->GetGameState()) : nullptr;
	};

	// --- Engine.TransitionToScene — UGameEngine 에 의존하므로 Game 모듈 측에서 추가 ---
	// (Engine 모듈의 LuaScriptManager 가 만든 "Engine" 테이블에 함수만 끼워 넣는다.)
	sol::table EngineTable = Lua["Engine"];
	EngineTable.set_function("TransitionToScene", [](const FString& Path)
	{
		// 다음 frame Tick 끝에 active world destroy + 새 scene 로드 + BeginPlay.
		// 호출 stack 위의 액터/Lua 컴포넌트가 destroy 되어 use-after-free 가 나지 않도록
		// deferred 처리 — UGameEngine::Tick 끝에서 ProcessPendingTransition 가 실행.
		// "Go To Intro" 등 동적 상태 전체 리셋 시 사용 (같은 scene 재로드도 OK — 액터 / PhysX
		// / 타이머 / 동적 스폰 모두 새 인스턴스로 시작).
		if (UGameEngine* Game = Cast<UGameEngine>(GEngine))
		{
			Game->RequestTransitionToScene(Path);
		}
	});
}

// ============================================================
// 자기-등록 — Editor / Game 측이 RegisterGameLuaBindings 함수명을 모르고도
// FEngineInitHooks::RunAll() 한 번이면 호출되도록 static initializer 로 등록.
// 호출 시점은 RunAll() — 그때면 FLuaScriptManager 가 이미 init 끝낸 상태.
// ============================================================
namespace
{
	void RunRegisterGameLuaBindings()
	{
		RegisterGameLuaBindings(FLuaScriptManager::GetState());
	}

	struct GameLuaBindingsAutoReg
	{
		GameLuaBindingsAutoReg() { FEngineInitHooks::Register(&RunRegisterGameLuaBindings); }
	};

	static GameLuaBindingsAutoReg gAutoReg;
}
