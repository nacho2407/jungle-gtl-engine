#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Runtime/Engine.h"

namespace FLuaEngineAPI
{
    void BindApplication(sol::state& Lua, sol::table& API)
    {
        sol::table Application = Lua.create_table();

        Application["QuitGame"] = []() -> bool
        {
            return GEngine && GEngine->RequestQuitGame();
        };

        API["Application"] = Application;
    }
}
