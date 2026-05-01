#pragma once

#include "Core/CoreTypes.h"
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

private:
	static void RegisterLuaHelpers(sol::state& Lua);
	static void RegisterCoreBindings(sol::state& Lua);
	static void RegisterMathBindings(sol::state& Lua);
	static void RegisterActorBindings(sol::state& Lua);

private:
	static std::unique_ptr<sol::state> Lua;
};
