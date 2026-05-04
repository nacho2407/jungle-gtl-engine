#pragma once

#include "Core/CoreTypes.h"
#include "Input/InputSystem.h"
#include <sol/sol.hpp>

class FLuaScriptManager
{
public:
	static void Initialize();
	static void Shutdown();

	static FString ResolveScriptPath(const FString& ScriptFile);
	static bool OpenOrCreateScript(const FString& ScriptFile);

	static sol::state& GetState();
	static void RegisterBindings(sol::state& Lua);

	static FInputSystemSnapshot GetLuaInputSnapshot();

	// World pause 와 무관하게 매 frame 발화되는 ESC 콜백. UIManager 가 등록하면 메뉴 토글이
	// pause 도중에도 동작한다 (component-tick 은 World pause 시 멈추므로 거기엔 못 둠).
	static void SetOnEscapePressed(sol::protected_function Callback);
	static void FireOnEscapePressed();

	// 씬 전환 시 호출. require 캐시된 모듈 (ObjRegistry / CoroutineManager) 이 보유한 stale
	// actor 포인터와 dangling 코루틴을 비운다. 안 하면 새 월드의 첫 Tick 에서 옛 코루틴이
	// Wait(30) 만료 후 재개되며 freed AActor* 를 deref → 크래시.
	static void FireWorldReset();

private:
	static void RegisterLuaHelpers(sol::state& Lua);
	static void RegisterCoreBindings(sol::state& Lua);
	static void RegisterMathBindings(sol::state& Lua);
	static void RegisterActorBindings(sol::state& Lua);
	static void RegisterUIBindings(sol::state& Lua);

private:
	static std::unique_ptr<sol::state> Lua;
	static sol::protected_function OnEscapePressedCallback;
};
