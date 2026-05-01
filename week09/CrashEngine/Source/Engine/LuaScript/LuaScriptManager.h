#pragma once
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class FLuaScriptManager : public TSingleton<FLuaScriptManager>
{
    friend class TSingleton<FLuaScriptManager>;

public:
    void Init();
    void Release();

    FString CreateScript(class AActor* TargetActor);
    bool DeleteScript(const FString& FileName);
    bool RenameScript(const FString& OldName, const FString& NewName);
    void OpenScriptInEditor(const FString& FileName);
    TArray<FString> GetAvailableScripts() const;

private:
    FLuaScriptManager() = default;
    ~FLuaScriptManager() = default;

    void OnLuaFileChanged(const FString& FileName);
};