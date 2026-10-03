#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Engine/Runtime/Engine.h"

namespace FLuaEngineAPI
{
    void BindScene(sol::state& Lua, sol::table& API)
    {
        sol::table Scene = Lua.create_table();

        Scene["Open"] = [](const FString& ScenePath) -> bool
        {
            return GEngine ? GEngine->RequestOpenScene(ScenePath) : false;
        };

        Scene["Reload"] = []() -> bool
        {
            return GEngine ? GEngine->RequestReloadScene() : false;
        };

        Scene["IsOpenPending"] = []() -> bool
        {
            return GEngine ? GEngine->IsSceneOpenPending() : false;
        };

        Scene["GetCurrentPath"] = []() -> FString
        {
            return GEngine ? GEngine->GetCurrentScenePath() : "";
        };

        API["Scene"] = Scene;
    }
}
