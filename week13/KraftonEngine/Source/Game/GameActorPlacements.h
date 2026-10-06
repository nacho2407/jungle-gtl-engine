#pragma once

// ============================================================
// Game 모듈이 Editor 의 "Place Actor" 메뉴에 자기 액터(예: ACarPawn) 를 추가하는
// 진입점. 호출 시점은 GameEngine / EditorEngine init 단계 — 어떤 ULuaScriptComponent
// 의 BeginPlay 보다 앞 (RegisterGameLuaBindings 와 동일 패턴).
//
// 헤더에는 Game 내부 클래스를 노출하지 않으므로 Editor/Engine 가 이 함수만 보고
// Game/Pawn/CarPawn.h 등을 include 할 필요가 없다.
// ============================================================
void RegisterGameActorPlacements();
