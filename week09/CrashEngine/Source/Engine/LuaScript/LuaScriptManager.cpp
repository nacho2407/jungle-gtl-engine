#include "LuaScriptManager.h"

#include "Component/LuaScriptComponent.h"
#include "Core/Watcher/DirectoryWatcher.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Platform/Paths.h"
#include "Object/ObjectIterator.h"

void FLuaScriptManager::Init()
{
    if (bWatcherRegistered)
    {
        return;
    }

    FDirectoryWatcher::Get().RegisterWatch(
        [this](const FString& FileName)
        {
            this->OnLuaFileChanged(FileName);
        });

    bWatcherRegistered = true;
}

FString FLuaScriptManager::CreateScript(class AActor* TargetActor)
{
    if (!TargetActor) return "";

    std::string scriptsPath = FPaths::ContentRelativePath("Scripts") + "/";
    int index = 0;

    std::string levelName = TargetActor->GetLevel()->GetFName().ToString();
    std::string actorName = TargetActor->GetFName().ToString();

    std::string fileName = levelName + "_" + actorName + "_" + std::to_string(index) + ".lua";
    std::string fullPath = scriptsPath + fileName;

    while (std::filesystem::exists(fullPath))
    {
        index++;
        fileName = levelName + "_" + actorName + "_" + std::to_string(index) + ".lua";
        fullPath = scriptsPath + fileName;
    }

    std::string templatePath = FPaths::ContentRelativePath("Scripts/Template.lua");
    if (std::filesystem::exists(templatePath))
    {
        std::filesystem::copy_file(templatePath, fullPath, std::filesystem::copy_options::overwrite_existing);
    }

    return FString(fileName.c_str());
}

void FLuaScriptManager::OnLuaFileChanged(const FString& FileName)
{
    if (FileName.find(".lua") == std::string::npos) return;

    if (FileName.find("Template.lua") != std::string::npos ||
        FileName.find("template.lua") != std::string::npos)
    {
        return;
    }

    for (TObjectIterator<ULuaScriptComponent> It; It; ++It)
    {
        ULuaScriptComponent* Component = *It;
        if (Component && Component->GetScriptPath() == FileName)
        {
            Component->ReloadScript();
        }
    }
}

bool FLuaScriptManager::DeleteScript(const FString& FileName)
{
    std::wstring WatchDir = FPaths::Combine(FPaths::ContentDir(), L"Scripts");
    std::wstring FullPath = FPaths::Combine(WatchDir, FPaths::ToWide(FileName));

    if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
    {
        return false;
    }

    std::error_code ec;
    if (std::filesystem::remove(FullPath, ec))
    {
        for (TObjectIterator<ULuaScriptComponent> It; It; ++It)
        {
            ULuaScriptComponent* Component = *It;
            if (Component && Component->GetScriptPath() == FileName)
            {
                Component->ClearScript();
            }
        }
        return true;
    }
    return false;
}

bool FLuaScriptManager::RenameScript(const FString& OldName, const FString& NewName)
{
    if (OldName == NewName || NewName.empty()) return false;

    std::wstring WatchDir = FPaths::Combine(FPaths::ContentDir(), L"Scripts");
    std::wstring OldPath = FPaths::Combine(WatchDir, FPaths::ToWide(OldName));
    std::wstring NewPath = FPaths::Combine(WatchDir, FPaths::ToWide(NewName));

    if (!std::filesystem::exists(OldPath)) return false;
    if (std::filesystem::exists(NewPath)) return false;

    std::error_code ec;
    std::filesystem::rename(OldPath, NewPath, ec);

    if (ec) return false;

    for (TObjectIterator<ULuaScriptComponent> It; It; ++It)
    {
        ULuaScriptComponent* Component = *It;
        if (Component && Component->GetScriptPath() == OldName)
        {
            Component->SetScriptPath(NewName);
        }
    }

    return true;
}

void FLuaScriptManager::OpenScriptInEditor(const FString& FileName)
{
    if (FileName.empty() || FileName == "None") return;

    std::string FullPath = FPaths::ContentRelativePath("Scripts") + "/" + FileName;
    std::string absoluteLuaScriptPath = std::filesystem::absolute(FullPath).string();

    std::wstring WidePath = FPaths::ToWide(absoluteLuaScriptPath);

    HINSTANCE hInstance = ShellExecuteW(
        NULL,
        L"open",
        WidePath.c_str(),
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)hInstance <= 32)
    {
        printf("Error: Failed to open script file: %s\n", FileName.c_str());
    }
}

TArray<FString> FLuaScriptManager::GetAvailableScripts() const
{
    TArray<FString> Scripts;

    std::wstring scriptsPath = FPaths::Combine(FPaths::ContentDir(), L"Scripts");
    if (!std::filesystem::exists(scriptsPath))
    {
        return Scripts;
    }

    for (const auto& Entry : std::filesystem::directory_iterator(scriptsPath))
    {
        if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
        {
            std::wstring WideFileName = Entry.path().filename().wstring();
            if (WideFileName != L"Template.lua" && WideFileName != L"template.lua")
            {
                Scripts.push_back(FPaths::ToUtf8(WideFileName));
            }
        }
    }
    return Scripts;
}