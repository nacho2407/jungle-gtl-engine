#pragma once
#include "Core/CoreTypes.h"
#include "LuaScript/LuaIncludes.h"

class FLuaGameObjectProxy;

namespace LuaActionLibrary
{
    void Wait(float WaitTime, sol::this_state CurrentState);
    void MoveTo(FLuaGameObjectProxy* Proxy, float TargetX, float TargetY, sol::this_state CurrentState);
    void WaitUntilMoveDone(FLuaGameObjectProxy* Proxy, sol::this_state CurrentState);
    bool RequestSceneLoad(const FString& ScenePath);
    bool ProcessPendingSceneLoad();
    void Bind(sol::state& Lua);
}
