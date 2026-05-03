#include "Game/Lua/GameLuaBindings.h"

#include "sol/sol.hpp"

#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Object/Object.h"  // Cast
#include "Core/Log.h"

#include "Game/Component/Movement/CarMovementComponent.h"
#include "Game/Component/CarGasComponent.h"
#include "Game/Component/DirtComponent.h"

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

	Lua.new_usertype<AGameStateCarGame>("GameStateCarGame",
		"GetPhase",              &AGameStateCarGame::GetPhase,
		"GetRemainingMatchTime", &AGameStateCarGame::GetRemainingMatchTime,
		"GetRemainingPhaseTime", &AGameStateCarGame::GetRemainingPhaseTime,
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

	Lua["GetGameState"] = []() -> AGameStateCarGame*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		return W ? Cast<AGameStateCarGame>(W->GetGameState()) : nullptr;
	};
}
