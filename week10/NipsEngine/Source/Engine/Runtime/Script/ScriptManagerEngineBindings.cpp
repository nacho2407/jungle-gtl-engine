#include "Runtime/Script/ScriptManager.h"

#include "Runtime/Script/API/LuaEngineAPIBindings.h"
#include "ThirdParty/sol/sol.hpp"

void FScriptManager::BindEngineAPI()
{
    if (!GLuaState)
    {
        return;
    }

    sol::table Engine = GLuaState->create_table();
    sol::table API = GLuaState->create_table();

    FLuaEngineAPI::BindTime(*GLuaState, API);
    FLuaEngineAPI::BindInput(*GLuaState, API);
    FLuaEngineAPI::BindSave(*GLuaState, API);
    FLuaEngineAPI::BindDebug(*GLuaState, API);
    FLuaEngineAPI::BindWorld(*GLuaState, API);
    FLuaEngineAPI::BindAudio(*GLuaState, API);
    FLuaEngineAPI::BindUI(*GLuaState, API);
    FLuaEngineAPI::BindAsset(*GLuaState, API);
    FLuaEngineAPI::BindRandom(*GLuaState, API);
    FLuaEngineAPI::BindApplication(*GLuaState, API);
    FLuaEngineAPI::BindEffect(*GLuaState, API);

    Engine["API"] = API;
    (*GLuaState)["Engine"] = Engine;
}
