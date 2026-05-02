#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"

#include "ThirdParty/sol/sol.hpp"

namespace FLuaEngineAPI
{
    void BindTime(sol::state& Lua, sol::table& API);
    void BindInput(sol::state& Lua, sol::table& API);
    void BindSave(sol::state& Lua, sol::table& API);
    void BindDebug(sol::state& Lua, sol::table& API);
    void BindWorld(sol::state& Lua, sol::table& API);
    void BindAudio(sol::state& Lua, sol::table& API);
    void BindUI(sol::state& Lua, sol::table& API);
    void BindAsset(sol::state& Lua, sol::table& API);
    void BindRandom(sol::state& Lua, sol::table& API);
    void BindApplication(sol::state& Lua, sol::table& API);
    void BindEffect(sol::state& Lua, sol::table& API);
}
