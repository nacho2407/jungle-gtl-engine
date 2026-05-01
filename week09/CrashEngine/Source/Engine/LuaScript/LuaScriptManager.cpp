#include "LuaScriptManager.h"
#include "Component/LuaScriptComponent.h"
#include "Core/Watcher/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Object/ObjectIterator.h"

void FLuaScriptManager::Init()
{
    FDirectoryWatcher::Get().RegisterWatch(
        [this](const FString& FileName) {
            this->OnLuaFileChanged(FileName);
        }
    );
}

void FLuaScriptManager::OnLuaFileChanged(const FString& FileName)
{
    if (FileName.find(".lua") == std::string::npos) return;

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
                Component->clearScript();
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