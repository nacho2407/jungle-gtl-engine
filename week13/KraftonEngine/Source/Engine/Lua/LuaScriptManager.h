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
