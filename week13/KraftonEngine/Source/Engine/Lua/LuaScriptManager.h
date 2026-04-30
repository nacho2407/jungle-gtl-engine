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
	static std::unique_ptr<sol::state> Lua;
};
