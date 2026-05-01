#include "DirectoryWatcher.h"
#include "Platform/Paths.h"

void FDirectoryWatcher::Init(const FString& InDirectoryToWatch)
{
    if (bIsRunning) return;

    WatchDirectory = InDirectoryToWatch;
    bIsRunning = true;

    WatcherThread = new std::thread(&FDirectoryWatcher::WatchFunction, this);
}

void FDirectoryWatcher::Release()
{
    bIsRunning = false;

    if (DirectoryHandle != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(DirectoryHandle, nullptr);
        CloseHandle(DirectoryHandle);
        DirectoryHandle = INVALID_HANDLE_VALUE;
    }

    if (WatcherThread && WatcherThread->joinable())
    {
        WatcherThread->join();
        delete WatcherThread;
        WatcherThread = nullptr;
    }
}

void FDirectoryWatcher::RegisterWatch(std::function<void(const FString&)> Callback)
{
    Listeners.push_back(Callback);
}

void FDirectoryWatcher::Tick()
{
    std::queue<FString> TempQueue;
    {
        std::lock_guard<std::mutex> Lock(QueueMutex);
        std::swap(TempQueue, ModifiedFilesQueue);
    }

    while (!TempQueue.empty())
    {
        FString ChangedFile = TempQueue.front();
        TempQueue.pop();

        for (const auto& Listener : Listeners)
        {
            if (Listener)
            {
                Listener(ChangedFile);
            }
        }
    }
}

void FDirectoryWatcher::WatchFunction()
{
    std::wstring WideWatchDir = FPaths::ToWide(WatchDirectory);

    DirectoryHandle = CreateFileW(
        WideWatchDir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (DirectoryHandle == INVALID_HANDLE_VALUE)
    {
        bIsRunning = false;
        return;
    }

    const DWORD BufferSize = 1024;
    uint8_t Buffer[BufferSize];
    DWORD BytesReturned;

    while (bIsRunning)
    {
        if (ReadDirectoryChangesW(
            DirectoryHandle,
            Buffer,
            BufferSize,
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &BytesReturned,
            NULL,
            NULL))
        {
            if (BytesReturned > 0)
            {
                FILE_NOTIFY_INFORMATION* NotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Buffer);

                do
                {
                    if (NotifyInfo->Action == FILE_ACTION_MODIFIED)
                    {
                        std::wstring WideFileName(NotifyInfo->FileName, NotifyInfo->FileNameLength / sizeof(WCHAR));
                        FString FileName = FPaths::ToUtf8(WideFileName);

                        std::lock_guard<std::mutex> Lock(QueueMutex);
                        ModifiedFilesQueue.push(FileName);
                    }

                    if (NotifyInfo->NextEntryOffset == 0) break;
                    NotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<uint8_t*>(NotifyInfo) + NotifyInfo->NextEntryOffset);
                } while (true);
            }
        }
        else
        {
            break;
        }
    }
}