#pragma once

#include "Core/CoreTypes.h"
#include "LuaScript/LuaIncludes.h"

class FLuaPersistentStorage
{
public:
    static void Bind(sol::state& Lua);
};
