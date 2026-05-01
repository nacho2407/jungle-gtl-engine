#pragma once
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <windows.h>

class ULuaScriptComponent;
class FLuaScriptManager : public TSingleton<FLuaScriptManager>
{
    friend class TSingleton<FLuaScriptManager>;

public:
    void Init();
    void Release();
    void Tick();

    void RegisterComponent(ULuaScriptComponent* Component);
    void UnRegisterComponent(ULuaScriptComponent* Component);
    bool DeleteScript(const FString& FileName);
    bool RenameScript(const FString& OldName, const FString& NewName);

    TArray<FString> GetAvailableScripts() const;

private:
    FLuaScriptManager() : bIsRunning(false), WatcherThread(nullptr){}
    ~FLuaScriptManager() { Release(); }

    void WatchFunction();

private:
    HANDLE DirectoryHandle = INVALID_HANDLE_VALUE;

    std::thread* WatcherThread;
    std::atomic<bool> bIsRunning;

    std::mutex QueueMutex;
    std::queue<FString> ModifiedFilesQueue;

    TArray<ULuaScriptComponent*> ActiveComponents;
    std::mutex ComponentMutex;
};