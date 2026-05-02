#pragma once

#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include "Core/Delegates/Delegate.h"

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class FDirectoryWatcher : public TSingleton<FDirectoryWatcher>
{
    friend class TSingleton<FDirectoryWatcher>;
public:
    DECLARE_DELEGATE(FOnDirectoryChanged, const FString&);
    FOnDirectoryChanged OnDirectoryChanged;

public:
    void Init(const FString& DirectoryToWatch);
    void Release();
    void Tick();

    void RegisterWatch(std::function<void(const FString&)> Callback);

private:
    FDirectoryWatcher() = default;
    ~FDirectoryWatcher() { Release(); }

    FDirectoryWatcher(const FDirectoryWatcher&) = delete;
    FDirectoryWatcher& operator=(const FDirectoryWatcher&) = delete;

    void WatchFunction();

private:
    HANDLE DirectoryHandle = INVALID_HANDLE_VALUE;
    std::thread* WatcherThread = nullptr;
    std::atomic<bool> bIsRunning = false;

    std::mutex QueueMutex;
    std::queue<FString> ModifiedFilesQueue;

    TArray<std::function<void(const FString&)>> Listeners;

    FString WatchDirectory;
};