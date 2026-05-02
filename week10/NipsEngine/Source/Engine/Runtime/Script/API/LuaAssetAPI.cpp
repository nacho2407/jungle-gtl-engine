#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Asset/AssetQueryService.h"

namespace
{
    sol::table StringsToLuaTable(sol::this_state State, const TArray<FString>& Strings)
    {
        sol::state_view Lua(State);
        sol::table Result = Lua.create_table();

        int32 Index = 1;
        for (const FString& String : Strings)
        {
            Result[Index++] = String;
        }

        return Result;
    }
}

namespace FLuaEngineAPI
{
    void BindAsset(sol::state& Lua, sol::table& API)
    {
        sol::table Asset = Lua.create_table();

        Asset["NormalizePath"] = [](sol::this_state State, const FString& Path) -> sol::object
        {
            FString NormalizedPath;
            if (!FAssetQueryService::NormalizeAssetPath(Path, NormalizedPath))
            {
                return sol::make_object(State, sol::nil);
            }
            return sol::make_object(State, NormalizedPath);
        };

        Asset["Exists"] = [](const FString& Path) -> bool
        {
            return FAssetQueryService::Exists(Path);
        };

        Asset["GetTexturePaths"] = [](sol::this_state State)
        {
            return StringsToLuaTable(State, FAssetQueryService::GetTexturePaths());
        };

        Asset["GetStaticMeshPaths"] = [](sol::this_state State)
        {
            return StringsToLuaTable(State, FAssetQueryService::GetStaticMeshPaths());
        };

        Asset["GetMaterialPaths"] = [](sol::this_state State)
        {
            return StringsToLuaTable(State, FAssetQueryService::GetMaterialPaths());
        };

        Asset["GetScenePaths"] = [](sol::this_state State)
        {
            return StringsToLuaTable(State, FAssetQueryService::GetScenePaths());
        };

        Asset["GetSoundPaths"] = [](sol::this_state State)
        {
            return StringsToLuaTable(State, FAssetQueryService::GetSoundPaths());
        };

        API["Asset"] = Asset;
    }
}
