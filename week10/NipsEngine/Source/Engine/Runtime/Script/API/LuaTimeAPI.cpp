#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Runtime/Engine.h"

namespace FLuaEngineAPI
{
    void BindTime(sol::state& Lua, sol::table& API)
    {
        sol::table Time = Lua.create_table();

        Time["SetTimeScale"] = [](float Scale)
        {
            if (GEngine)
            {
                GEngine->SetTimeScale(Scale);
            }
        };

        Time["GetTimeScale"] = []() -> float
        {
            return GEngine ? GEngine->GetTimeScale() : 1.0f;
        };

        Time["GetDeltaTime"] = []() -> float
        {
            return GEngine ? GEngine->GetDeltaTime() : 0.0f;
        };

        Time["GetUnscaledDeltaTime"] = []() -> float
        {
            return GEngine ? GEngine->GetUnscaledDeltaTime() : 0.0f;
        };

        Time["GetGameTime"] = []() -> double
        {
            return GEngine ? GEngine->GetGameTime() : 0.0;
        };

        Time["GetRealTime"] = []() -> double
        {
            return GEngine ? GEngine->GetRealTime() : 0.0;
        };

        API["Time"] = Time;
    }
}
