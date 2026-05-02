#include "ScriptManager.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "GameFramework/AActor.h"
#include "Runtime/Script/ScriptComponent.h"
#include "ThirdParty/sol/sol.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <shellapi.h>

namespace fs = std::filesystem;

void Log(const std::string& Msg);

namespace
{
    FWString AssetScriptDir()
    {
        return FPaths::Combine(FPaths::RootDir(), L"Asset/Script");
    }

    TArray<FWString> GetScriptSearchDirs()
    {
        TArray<FWString> Dirs;
        Dirs.push_back(FPaths::ScriptDir());
        Dirs.push_back(AssetScriptDir());
        return Dirs;
    }

    bool IsBlankString(const FString& Value)
    {
        return std::all_of(
            Value.begin(),
            Value.end(),
            [](unsigned char Ch)
            {
                return std::isspace(Ch) != 0;
            });
    }

    FName MakeLuaScriptKey(const FString& ScriptName)
    {
        std::filesystem::path ScriptPath(FPaths::ToWide(ScriptName));
        FString Key = FPaths::ToUtf8(ScriptPath.stem().generic_wstring());
        if (Key.empty())
        {
            Key = ScriptName;
        }
        return FName(Key);
    }

    FString NormalizeLuaPath(const FWString& Path)
    {
        FString Result = FPaths::ToUtf8(Path);
        std::replace(Result.begin(), Result.end(), '\\', '/');
        if (!Result.empty() && Result.back() != '/')
        {
            Result += "/";
        }
        return Result;
    }

    FString MakeRelativeScriptDisplayPath(const fs::path& Path)
    {
        return FPaths::ToRelativeString(Path.wstring());
    }

    fs::path MakeScriptCreatePath(FString ScriptName)
    {
        if (!ScriptName.ends_with(".lua"))
        {
            ScriptName += ".lua";
        }

        fs::path RequestedPath(FPaths::ToWide(ScriptName));
        if (!RequestedPath.has_parent_path())
        {
            RequestedPath = fs::path(AssetScriptDir()) / RequestedPath;
        }
        else if (!RequestedPath.is_absolute())
        {
            RequestedPath = fs::path(FPaths::RootDir()) / RequestedPath;
        }

        return RequestedPath.lexically_normal();
    }

    bool TryResolveExistingScriptPath(const FString& ScriptName, fs::path& OutPath)
    {
        if (ScriptName.empty() || IsBlankString(ScriptName))
        {
            return false;
        }

        FString FileName = ScriptName;
        if (!FileName.ends_with(".lua"))
        {
            FileName += ".lua";
        }

        fs::path RequestedPath(FPaths::ToWide(FileName));
        if (RequestedPath.is_absolute())
        {
            RequestedPath = RequestedPath.lexically_normal();
            if (fs::exists(RequestedPath) && fs::is_regular_file(RequestedPath))
            {
                OutPath = RequestedPath;
                return true;
            }
        }
        else if (RequestedPath.has_parent_path())
        {
            RequestedPath = (fs::path(FPaths::RootDir()) / RequestedPath).lexically_normal();
            if (fs::exists(RequestedPath) && fs::is_regular_file(RequestedPath))
            {
                OutPath = RequestedPath;
                return true;
            }
        }

        for (const FWString& SearchDir : GetScriptSearchDirs())
        {
            fs::path Candidate = (fs::path(SearchDir) / FPaths::ToWide(FileName)).lexically_normal();
            if (fs::exists(Candidate) && fs::is_regular_file(Candidate))
            {
                OutPath = Candidate;
                return true;
            }
        }

        return false;
    }
}

void FScriptManager::initializeLuaState()
{
    GLuaState = std::make_unique<sol::state>();
    GLuaState->open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::os,
        sol::lib::math,
        sol::lib::table,
        sol::lib::bit32,
        sol::lib::utf8);

    ConfigureLuaPackagePath();

    RefreshLuaScriptFiles();
    BindLuaState();
}

void FScriptManager::ShutdownLuaState()
{
    if (!GLuaState)
    {
        return;
    }

    for (auto& Pair : ScriptArray)
    {
        FLuaScriptInfo& ScriptInfo = Pair.second;
        for (UScriptComponent* ScriptComponent : ScriptInfo.ScriptComponents)
        {
            if (ScriptComponent)
            {
                ScriptComponent->ReleaseLuaStateReferences();
            }
        }
        ScriptInfo.ScriptComponents.clear();
    }

    GLuaState.reset();
}

void FScriptManager::ConfigureLuaPackagePath()
{
    if (!GLuaState)
    {
        return;
    }

    sol::table Package = (*GLuaState)["package"];
    const FString CurrentPath = Package["path"].get_or(FString());

    FString ExtraPath;
    for (const FWString& ScriptDir : GetScriptSearchDirs())
    {
        const FString NormalizedDir = NormalizeLuaPath(ScriptDir);
        ExtraPath += NormalizedDir + "?.lua;";
        ExtraPath += NormalizedDir + "?/init.lua;";
    }

    if (CurrentPath.find(ExtraPath) == FString::npos)
    {
        Package["path"] = ExtraPath + CurrentPath;
    }
}

void FScriptManager::BindLuaState()
{
    BindMathTypes();
    BindObjectTypes();
    BindComponentTypes();
    BindActorTypes();
    BindStaticMeshTypes();
    BindBillboardTypes();
    BindCameraTypes();
    BindPrimitiveTypes();
    BindDecalTypes();
    BindEngineAPI();

    GLuaState->set_function("Log", &Log);
}

void Log(const std::string& Msg)
{
    UE_LOG("[Lua] %s", Msg.c_str());
}

bool FScriptManager::CreateScript(const FName& LuaScriptName)
{
    if (!LuaScriptName.IsValid() || IsBlankString(LuaScriptName.ToString()))
    {
        UE_LOG("[LuaManager] Script name is empty.");
        return false;
    }

    FString ScriptName = LuaScriptName.ToString();

    FWString TemplatePath = FPaths::LuaTemplatePath();
    fs::path ScriptPath = MakeScriptCreatePath(ScriptName);

    if (!fs::exists(TemplatePath))
    {
        MessageBoxW(nullptr, TemplatePath.c_str(), L"Path", MB_OK);
        UE_LOG("[LuaManager] 템플릿 파일이 존재하지 않습니다: %s", TemplatePath.c_str());
        return false;
    }

    if (fs::exists(ScriptPath))
    {
        UE_LOG("[LuaManager] 스크립트 파일이 이미 존재합니다: %s", ScriptPath.c_str());
        return false;
    }

    try
    {
        fs::create_directories(ScriptPath.parent_path());
        fs::copy_file(
            TemplatePath,
            ScriptPath,
            fs::copy_options::none);
    }
    catch (const fs::filesystem_error& e)
    {
        UE_LOG("[LuaManager] 스크립트 복사 실패: %s", e.what());
        return false;
    }

    const FString RelativeScriptPath = MakeRelativeScriptDisplayPath(ScriptPath);
    const FName ScriptKey = MakeLuaScriptKey(RelativeScriptPath);
    FLuaScriptInfo& Info = ScriptArray[ScriptKey];
    Info.ScriptPath = ScriptPath.wstring();
    Info.LastWriteTime = fs::last_write_time(ScriptPath);

    RefreshLuaScriptFiles();
    UE_LOG("[LuaManager] 스크립트 생성 완료: %s", ScriptPath.c_str());
    return true;
}

bool FScriptManager::EditScript(const FName& LuaScriptName)
{
    if (!LuaScriptName.IsValid())
    {
        UE_LOG("[LuaManager] 선택된 스크립트가 없습니다.");
        return false;
    }

    FString ResolvedPathText;
    if (!ResolveScriptPath(LuaScriptName.ToString(), ResolvedPathText))
    {
        UE_LOG("[LuaManager] Script file not found: %s", LuaScriptName.ToString().c_str());
        return false;
    }
    FWString ScriptPath = FPaths::ToWide(ResolvedPathText);

    HINSTANCE InstanceHandle = ShellExecute(nullptr, L"open", ScriptPath.data(),
                                             nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)InstanceHandle <= 32)
    {
        MessageBoxW(NULL, ScriptPath.c_str(), L"Path", MB_OK | MB_ICONERROR);
        UE_LOG("[LuaManager] 스크립트 파일을 열 수 없습니다: %s", ScriptPath.c_str());
        return false;
    }
    return true;
}

bool FScriptManager::HasScript(const FName& name)
{
    fs::path ResolvedPath;
    if (TryResolveExistingScriptPath(name.ToString(), ResolvedPath))
    {
        return true;
    }

    auto It = ScriptArray.find(MakeLuaScriptKey(name.ToString()));
    return It != ScriptArray.end()
        && !It->second.ScriptPath.empty()
        && fs::exists(It->second.ScriptPath);
}

bool FScriptManager::ResolveScriptPath(const FString& ScriptName, FString& OutPath)
{
    if (ScriptName.empty())
    {
        return false;
    }

    fs::path ResolvedPath;
    if (TryResolveExistingScriptPath(ScriptName, ResolvedPath))
    {
        OutPath = FPaths::ToUtf8(ResolvedPath.wstring());
        return true;
    }

    const FName ScriptKey = MakeLuaScriptKey(ScriptName);
    auto It = ScriptArray.find(ScriptKey);
    if (It != ScriptArray.end() && !It->second.ScriptPath.empty() && fs::exists(It->second.ScriptPath))
    {
        OutPath = FPaths::ToUtf8(It->second.ScriptPath);
        return true;
    }

    UE_LOG("[ScriptManager] Script file not found: %s", ScriptName.c_str());
    return false;
}

void FScriptManager::HotReloadScripts()
{
    RefreshLuaScriptFiles();

    for (auto& [ScriptKey, ScriptInfo] : ScriptArray)
    {
        if (!ScriptInfo.ScriptPath.empty() && fs::exists(ScriptInfo.ScriptPath))
        {
            auto LastWriteTime = fs::last_write_time(ScriptInfo.ScriptPath);

            if (LastWriteTime > ScriptInfo.LastWriteTime)
            {
                UE_LOG("[ScriptManager] 핫 리로드 실행");
                bool bReloadSuccess = true;
                for (auto* ScriptComponent : ScriptInfo.ScriptComponents)
                {
                    if (!ScriptComponent)
                    {
                        continue;
                    }

                    UE_LOG("[ScriptManager] 핫리로드 대상 %s", ScriptComponent->GetName().c_str());
                    bool bResult = ScriptComponent->HotReloadScript();
                    bReloadSuccess = bReloadSuccess && bResult;
                }
                if (bReloadSuccess)
                {
                    ScriptInfo.LastWriteTime = LastWriteTime;
                }
            }
        }
    }
}

void FScriptManager::RefreshLuaScriptFiles()
{
    for (auto It = ScriptArray.begin(); It != ScriptArray.end();)
    {
        if (!It->second.ScriptPath.empty() && !fs::exists(It->second.ScriptPath))
        {
            It = ScriptArray.erase(It);
            continue;
        }
        ++It;
    }

    for (const FWString& ScriptDir : GetScriptSearchDirs())
    {
        std::error_code Ec;
        fs::create_directories(ScriptDir, Ec);
        if (!fs::exists(ScriptDir) || !fs::is_directory(ScriptDir))
        {
            continue;
        }

        for (const auto& Entry : fs::recursive_directory_iterator(ScriptDir))
        {
            if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
            {
                FString ScriptName = MakeRelativeScriptDisplayPath(Entry.path());
                FName ScriptKey = MakeLuaScriptKey(ScriptName);

                FLuaScriptInfo& Info = ScriptArray[ScriptKey];
                Info.ScriptPath = Entry.path().wstring();
                if (Info.LastWriteTime == fs::file_time_type::min())
                {
                    Info.LastWriteTime = fs::last_write_time(Entry.path());
                }
            }
        }
    }
}

void FScriptManager::RegisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
    {
        return;
    }

    FName ScriptName = MakeLuaScriptKey(name);

    FLuaScriptInfo& ScriptInfo = ScriptArray[ScriptName];

    if (ScriptInfo.ScriptPath.empty())
    {
        FString ResolvedPath;
        if (ResolveScriptPath(name, ResolvedPath))
        {
            ScriptInfo.ScriptPath = FPaths::ToWide(ResolvedPath);
        }
    }

    auto& Components = ScriptInfo.ScriptComponents;

    auto It = std::find(
        Components.begin(),
        Components.end(),
        ScriptComponent);

    if (It == Components.end())
    {
        Components.push_back(ScriptComponent);
    }
}

void FScriptManager::UnregisterScriptComponents(const FString& name, UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
    {
        return;
    }

    FName ScriptName = MakeLuaScriptKey(name);
    auto It = ScriptArray.find(ScriptName);
    if (It == ScriptArray.end())
    {
        return;
    }

    auto& Components = It->second.ScriptComponents;
    Components.erase(
        std::remove(Components.begin(), Components.end(), ScriptComponent),
        Components.end());
}

void FScriptManager::UnregisterScriptComponentAll(UScriptComponent* ScriptComponent)
{
    if (!ScriptComponent)
    {
        return;
    }

    for (auto It = ScriptArray.begin(); It != ScriptArray.end();)
    {
        auto& Components = It->second.ScriptComponents;

        Components.erase(
            std::remove(Components.begin(), Components.end(), ScriptComponent),
            Components.end());

        ++It;
    }
}

std::optional<FLuaScriptLoadResult> FScriptManager::LoadScriptClass(
    UScriptComponent* Component,
    const FString& ScriptName)
{
    FString ScriptPath;
    if (!ResolveScriptPath(ScriptName, ScriptPath))
    {
        UE_LOG("[ScriptManager] Script not found: %s", ScriptName.c_str());
        return std::nullopt;
    }

    sol::environment Env(*GLuaState, sol::create, GLuaState->globals());

    Env["Component"] = Component;
    Env["Actor"] = Component ? Component->GetOwner() : nullptr;
    Env["Owner"] = Component ? Component->GetOwner() : nullptr;

    sol::protected_function_result Result =
        GLuaState->safe_script_file(ScriptPath, Env);

    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG("[ScriptManager] Lua load error: %s", Err.what());
        return std::nullopt;
    }

    sol::object ReturnObj = Result;
    if (!ReturnObj.valid() || ReturnObj.get_type() != sol::type::table)
    {
        UE_LOG("[ScriptManager] Script must return table: %s", ScriptName.c_str());
        return std::nullopt;
    }

    FLuaScriptLoadResult Loaded;
    Loaded.Env = std::move(Env);
    Loaded.ScriptClass = ReturnObj.as<sol::table>();
    return Loaded;
}

std::optional<sol::table> FScriptManager::LoadScriptClassForProperties(
    const FString& ScriptName)
{
    FString ScriptPath;
    if (!ResolveScriptPath(ScriptName, ScriptPath))
    {
        return std::nullopt;
    }

    sol::environment TempEnv(*GLuaState, sol::create, GLuaState->globals());

    sol::protected_function_result Result =
        GLuaState->safe_script_file(ScriptPath, TempEnv);

    if (!Result.valid())
    {
        sol::error Err = Result;
        UE_LOG("[ScriptManager] Lua property load error: %s", Err.what());
        return std::nullopt;
    }

    sol::object ReturnObj = Result;
    if (!ReturnObj.valid() || ReturnObj.get_type() != sol::type::table)
    {
        return std::nullopt;
    }

    return ReturnObj.as<sol::table>();
}

FWString FScriptManager::GetScriptPathByName(const FName& name)
{
    auto Info = GetScriptInfo(name);
    return Info ? Info->ScriptPath : FWString();
}

auto FScriptManager::GetScriptInfo(const FName& name) -> FLuaScriptInfo*
{
    auto It = ScriptArray.find(MakeLuaScriptKey(name.ToString()));

    if (It == ScriptArray.end())
    {
        return nullptr;
    }

    return &It->second;
}
