#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Core/Logging/Log.h"

namespace FLuaEngineAPI
{
    void BindDebug(sol::state& Lua, sol::table& API)
    {
        sol::table Debug = Lua.create_table();

        Debug["Log"] = [](const FString& Message)
        {
            UE_LOG("[Lua] %s", Message.c_str());
        };

        Debug["Warn"] = [](const FString& Message)
        {
            UE_LOG("[Lua Warning] %s", Message.c_str());
        };

        Debug["Error"] = [](const FString& Message)
        {
            UE_LOG("[Lua Error] %s", Message.c_str());
        };

        API["Debug"] = Debug;
    }
}
