#include "ScriptManager.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "GameFramework/AActor.h"
#include "Runtime/Script/ScriptComponent.h"
#include "ThirdParty/sol/sol.hpp"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <sstream>

namespace fs = std::filesystem;

void Log(const std::string& Msg);

namespace
{
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
        return Result;
    }

    fs::path WithLuaExtension(fs::path Path)
    {
        if (Path.extension() != ".lua")
        {
            Path += L".lua";
        }
        return Path;
    }

    bool TryResolveLuaScriptPath(const FString& ScriptName, fs::path& OutPath)
    {
        if (ScriptName.empty())
        {
            return false;
        }

        fs::path Requested = WithLuaExtension(fs::path(FPaths::ToWide(ScriptName)));
        TArray<fs::path> Candidates;

        if (Requested.is_absolute())
        {
            Candidates.push_back(Requested.lexically_normal());
        }
        else
        {
            if (Requested.has_parent_path())
            {
                Candidates.push_back((fs::path(FPaths::RootDir()) / Requested).lexically_normal());
            }

            Candidates.push_back((fs::path(FPaths::ScriptDir()) / Requested.filename()).lexically_normal());
        }

        for (const fs::path& Candidate : Candidates)
        {
            if (fs::exists(Candidate) && fs::is_regular_file(Candidate))
            {
                OutPath = Candidate;
                return true;
            }
        }

        if (!Candidates.empty())
        {
            OutPath = Candidates.front();
        }
        return false;
    }

    bool ReadLuaScriptFile(const FString& ScriptPath, FString& OutSource)
    {
        std::ifstream File(fs::path(FPaths::ToWide(ScriptPath)), std::ios::binary);
        if (!File.is_open())
        {
            return false;
        }

        std::ostringstream Stream;
        Stream << File.rdbuf();
        OutSource = Stream.str();
        return true;
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

    ScriptArray.clear();
    GLuaState.reset();
}

void FScriptManager::ConfigureLuaPackagePath()
{
    if (!GLuaState)
    {
        return;
    }

    const FString ScriptDir = NormalizeLuaPath(FPaths::ScriptDir());
    sol::table Package = (*GLuaState)["package"];
    const FString CurrentPath = Package["path"].get_or(FString());

    FString ExtraPath;
    ExtraPath += ScriptDir + "?.lua;";
    ExtraPath += ScriptDir + "?/init.lua;";

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
    FString ScriptName;
    if (!LuaScriptName.IsValid())
    {
        ScriptName = "Actor.lua";
    }
    else
    {
        ScriptName = LuaScriptName.ToString();

        if (!ScriptName.ends_with(".lua"))
        {
            ScriptName += ".lua";
        }
    }

    FWString TemplatePath = FPaths::LuaTemplatePath();
    FWString ScriptPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptName));

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

    const FName ScriptKey = MakeLuaScriptKey(ScriptName);
    if (ScriptArray.find(ScriptKey) != ScriptArray.end())
    {
        ScriptArray[ScriptKey].ScriptPath = ScriptPath;
        ScriptArray[ScriptKey].LastWriteTime = fs::last_write_time(ScriptPath);
    }

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

    FWString ScriptPath;
    const FName ScriptKey = MakeLuaScriptKey(LuaScriptName.ToString());
    if (ScriptArray.find(ScriptKey) != ScriptArray.end())
    {
        ScriptPath = ScriptArray[ScriptKey].ScriptPath;
    }
    else
    {
        FString ScriptName = LuaScriptName.ToString();
        if (!ScriptName.ends_with(".lua"))
        {
            ScriptName += ".lua";
        }

        ScriptPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptName));
    }

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
    auto It = ScriptArray.find(MakeLuaScriptKey(name.ToString()));

    if (It == ScriptArray.end())
    {
        return false;
    }
    return true;
}

bool FScriptManager::ResolveScriptPath(const FString& ScriptName, FString& OutPath)
{
    fs::path Path;
    if (!TryResolveLuaScriptPath(ScriptName, Path))
    {
        UE_LOG("[ScriptManager] Script file not found: %s", FPaths::ToUtf8(Path.wstring()).c_str());
        return false;
    }

    OutPath = FPaths::ToUtf8(Path.wstring());
    return true;
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
    FWString ScriptDir = FPaths::ScriptDir();
    if (!fs::exists(ScriptDir))
    {
        return;
    }

    for (const auto& Entry : fs::recursive_directory_iterator(ScriptDir))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
        {
            FString ScriptName = FPaths::ToUtf8(Entry.path().stem().generic_wstring());
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
        fs::path Path;
        if (TryResolveLuaScriptPath(name, Path))
        {
            ScriptInfo.ScriptPath = Path.wstring();
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

    FString ScriptSource;
    if (!ReadLuaScriptFile(ScriptPath, ScriptSource))
    {
        UE_LOG("[ScriptManager] Failed to read script file: %s", ScriptPath.c_str());
        return std::nullopt;
    }

    sol::protected_function_result Result =
        GLuaState->safe_script(ScriptSource, Env);

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

    FString ScriptSource;
    if (!ReadLuaScriptFile(ScriptPath, ScriptSource))
    {
        UE_LOG("[ScriptManager] Failed to read script file for properties: %s", ScriptPath.c_str());
        return std::nullopt;
    }

    sol::protected_function_result Result =
        GLuaState->safe_script(ScriptSource, TempEnv);

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
