#include "Runtime/Script/API/LuaEngineAPIBindings.h"

namespace FLuaEngineAPI
{
    void BindEffect(sol::state& Lua, sol::table& API)
    {
        API["Effect"] = Lua.create_table();
    }
}
