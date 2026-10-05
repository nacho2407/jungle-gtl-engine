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

    FLuaEngineAPI::BindInput(*GLuaState, API);
    FLuaEngineAPI::BindSave(*GLuaState, API);
    FLuaEngineAPI::BindJson(*GLuaState, API);
    FLuaEngineAPI::BindScene(*GLuaState, API);
    FLuaEngineAPI::BindDebug(*GLuaState, API);
    FLuaEngineAPI::BindWorld(*GLuaState, API);
    sol::table World = API["World"];
    API["GetPlayerController"] = World["GetPlayerController"];
    API["GetPossessedActor"] = World["GetPossessedActor"];
    API["GetViewTargetActor"] = World["GetViewTargetActor"];
    API["GetViewTargetCamera"] = World["GetViewTargetCamera"];
    FLuaEngineAPI::BindAudio(*GLuaState, API);
    FLuaEngineAPI::BindUI(*GLuaState, API);
    FLuaEngineAPI::BindAsset(*GLuaState, API);
    FLuaEngineAPI::BindRandom(*GLuaState, API);
    FLuaEngineAPI::BindApplication(*GLuaState, API);
    FLuaEngineAPI::BindEffect(*GLuaState, API);

    Engine["API"] = API;
    (*GLuaState)["Engine"] = Engine;
}
