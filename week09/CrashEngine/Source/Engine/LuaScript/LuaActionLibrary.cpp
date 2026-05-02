#include "LuaActionLibrary.h"

#include "Core/CoroutineScheduler/LuaCoroutineScheduler.h"
#include "LuaScript/LuaGameObjectProxy.h"

namespace LuaActionLibrary
{
    void Wait(float WaitTime, sol::this_state CurrentState)
    {
        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        FLuaCoroutineScheduler::Get().AddTimeTask(CurrentThread, WaitTime);
    }

    void MoveTo(FLuaGameObjectProxy* Proxy, float TargetX, float TargetY, sol::this_state CurrentState)
    {
        if (!Proxy || !Proxy->IsValid()) return;

        FVector NewLoc = Proxy->GetLocation();
        NewLoc.X = TargetX;
        NewLoc.Y = TargetY;
        Proxy->SetLocation(NewLoc);

        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        auto Condition = [Proxy]() -> bool {
            if (!Proxy || !Proxy->IsValid()) return true;
            return Proxy->Velocity.LengthSquared() <= 0.01f;
            };

        FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);
    }

    void WaitUntilMoveDone(FLuaGameObjectProxy* Proxy, sol::this_state CurrentState)
    {
        if (!Proxy || !Proxy->IsValid()) return;

        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        auto Condition = [Proxy]() -> bool {
            if (!Proxy || !Proxy->IsValid()) return true;
            return Proxy->Velocity.LengthSquared() <= 0.01f;
            };

        FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);
    }

    void Bind(sol::state& Lua)
    {
        Lua.set_function("Wait", sol::yielding(&LuaActionLibrary::Wait));
        Lua.set_function("MoveTo", sol::yielding(&LuaActionLibrary::MoveTo));
        Lua.set_function("WaitUntilMoveDone", sol::yielding(&LuaActionLibrary::WaitUntilMoveDone));
    }
}