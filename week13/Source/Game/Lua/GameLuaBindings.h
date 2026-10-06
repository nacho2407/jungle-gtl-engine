#pragma once

namespace sol { class state; }

// ============================================================
// Game 모듈 전용 Lua 바인딩 등록.
//
// Engine 의 FLuaScriptManager 는 Engine 일반 타입(AActor, APawn, FVector, World 등)
// 까지만 등록한다. Game-특화 타입(ACarPawn, AGameStateCarGame, ECarGamePhase 등) 과
// AActor 의 game-extension 메서드(AsCarPawn, GetCarMovement 등) 는 이 함수가 등록.
//
// 호출 시점: UEngine::Init() 이 FLuaScriptManager::Initialize() 를 끝낸 직후.
// GameEngine / EditorEngine 두 엔트리 모두 호출 — PIE 에서도 game 스크립트가
// 동일 바인딩으로 동작해야 하므로.
// ============================================================
void RegisterGameLuaBindings(sol::state& Lua);
