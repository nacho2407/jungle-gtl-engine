#pragma once
#include "LuaScript/LuaIncludes.h"

class FLuaGameObjectProxy;

namespace LuaActionLibrary
{
    void Wait(float WaitTime, sol::this_state CurrentState);
    void MoveTo(FLuaGameObjectProxy* Proxy, float TargetX, float TargetY, sol::this_state CurrentState);
    void WaitUntilMoveDone(FLuaGameObjectProxy* Proxy, sol::this_state CurrentState);

    void Bind(sol::state& Lua);
}