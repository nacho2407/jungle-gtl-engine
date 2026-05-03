#pragma once

#include "Core/CoreTypes.h"

// ============================================================
// FEngineInitHooks — 모듈 init 함수 셀프-등록 레지스트리.
//
// Game 등 외부 모듈이 자기 init 진입점(예: RegisterGameLuaBindings, RegisterGame
// ActorPlacements) 을 static initializer 로 Register() 해 두면, Engine 부팅 시
// RunAll() 한 번이 모두 호출한다. 결과적으로 Editor / Game-Engine 양쪽이 Game
// 모듈 헤더를 직접 include 하거나 함수명을 알 필요가 없다.
//
// 사용 패턴 (Game 측 .cpp 끝):
//   namespace {
//       void RunMyInit() { RegisterMyBindings(SomeSubsystem::Get()); }
//       struct AutoReg { AutoReg() { FEngineInitHooks::Register(&RunMyInit); } };
//       static AutoReg gAutoReg;
//   }
//
// 주의:
//  - 등록되는 함수는 parameterless. 인자 필요한 init 은 wrapper 에서 Get() 으로 가져옴.
//  - 등록 시점에는 등록만 — 실제 호출은 RunAll() 에서. 그래서 Lua/Audio 등 다른
//    엔진 서브시스템이 아직 init 안 되어 있어도 안전.
//  - Meyer's singleton (`GetRegistry()` 의 함수-로컬 static) 으로 정적 초기화 순서
//    문제 회피. 다른 TU 의 static 생성자에서 Register() 호출돼도 OK.
//  - 등록된 .cpp 는 vcxproj 에 명시 컴파일되어야 함 (현재 모두 그러함).
//    obj 가 통째로 dead-strip 되면 static initializer 가 안 도는데, 명시 등록 시
//    MSVC 는 그대로 링크.
// ============================================================
class FEngineInitHooks
{
public:
	using FInitFn = void(*)();

	static void Register(FInitFn Fn);
	static void RunAll();

private:
	static TArray<FInitFn>& GetRegistry();
};
