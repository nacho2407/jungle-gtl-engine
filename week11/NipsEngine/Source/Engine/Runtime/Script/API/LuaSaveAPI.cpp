#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"

#include <filesystem>
#include <fstream>

namespace
{
    bool ResolveSafeSavePath(const FString& RelativePath, std::filesystem::path& OutPath)
    {
        if (RelativePath.empty())
        {
            return false;
        }

        std::filesystem::path RawPath(FPaths::ToWide(RelativePath));
        if (RawPath.is_absolute())
        {
            return false;
        }

        std::filesystem::path CleanRelative;
        bool bSkippedLeadingSaves = false;
        for (const std::filesystem::path& Part : RawPath.lexically_normal())
        {
            const std::wstring PartString = Part.wstring();
            if (PartString.empty() || PartString == L".")
            {
                continue;
            }
            if (PartString == L"..")
            {
                return false;
            }
            if (!bSkippedLeadingSaves && CleanRelative.empty() && (PartString == L"Saves" || PartString == L"saves"))
            {
                bSkippedLeadingSaves = true;
                continue;
            }

            CleanRelative /= Part;
        }

        if (CleanRelative.empty())
        {
            return false;
        }

        OutPath = (std::filesystem::path(FPaths::RootDir()) / L"Saves" / CleanRelative).lexically_normal();
        return true;
    }
}

namespace FLuaEngineAPI
{
    void BindSave(sol::state& Lua, sol::table& API)
    {
        sol::table Save = Lua.create_table();

        Save["WriteText"] = [](const FString& RelativePath, const FString& Text) -> bool
        {
            std::filesystem::path SavePath;
            if (!ResolveSafeSavePath(RelativePath, SavePath))
            {
                UE_LOG("[Engine.API.Save] Invalid save path: %s", RelativePath.c_str());
                return false;
            }

            std::error_code Ec;
            std::filesystem::create_directories(SavePath.parent_path(), Ec);
            if (Ec)
            {
                UE_LOG("[Engine.API.Save] Failed to create save directory: %s", FPaths::ToUtf8(SavePath.parent_path().wstring()).c_str());
                return false;
            }

            std::ofstream File(SavePath, std::ios::binary | std::ios::trunc);
            if (!File.is_open())
            {
                UE_LOG("[Engine.API.Save] Failed to open save file for write: %s", FPaths::ToUtf8(SavePath.wstring()).c_str());
                return false;
            }

            File << Text;
            return true;
        };

        Save["ReadText"] = [](sol::this_state State, const FString& RelativePath) -> sol::object
        {
            std::filesystem::path SavePath;
            if (!ResolveSafeSavePath(RelativePath, SavePath))
            {
                UE_LOG("[Engine.API.Save] Invalid save path: %s", RelativePath.c_str());
                return sol::make_object(State, sol::nil);
            }

            std::ifstream File(SavePath, std::ios::binary);
            if (!File.is_open())
            {
                return sol::make_object(State, sol::nil);
            }

            FString Text((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
            return sol::make_object(State, Text);
        };

        Save["Exists"] = [](const FString& RelativePath) -> bool
        {
            std::filesystem::path SavePath;
            return ResolveSafeSavePath(RelativePath, SavePath) && std::filesystem::exists(SavePath);
        };

        Save["Delete"] = [](const FString& RelativePath) -> bool
        {
            std::filesystem::path SavePath;
            if (!ResolveSafeSavePath(RelativePath, SavePath))
            {
                UE_LOG("[Engine.API.Save] Invalid save path: %s", RelativePath.c_str());
                return false;
            }

            std::error_code Ec;
            const bool bRemoved = std::filesystem::remove(SavePath, Ec);
            return !Ec && bRemoved;
        };

        API["Save"] = Save;
    }
}
