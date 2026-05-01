#include "LuaScriptManager.h"
#include "Component/LuaScriptComponent.h"
#include "Platform/Paths.h"

const std::string SCRIPT_DIRECTORY = "Asset/Content/Scripts";

void FLuaScriptManager::Init()
{
	if (bIsRunning) return;
	bIsRunning = true;
	WatcherThread = new std::thread(&FLuaScriptManager::WatchFunction, this);
}

void FLuaScriptManager::Release()
{
	bIsRunning = false;

	if (DirectoryHandle != INVALID_HANDLE_VALUE)
	{
		CancelIoEx(DirectoryHandle, NULL);
	}

    if (WatcherThread && WatcherThread->joinable())
	{
		WatcherThread->join();
		delete WatcherThread;
		WatcherThread = nullptr;
	}
}

void FLuaScriptManager::Tick()
{
	TArray<FString> FilesToReload;
	{
		std::lock_guard<std::mutex> Lock(QueueMutex);
		while (!ModifiedFilesQueue.empty())
		{
			FString FileName = ModifiedFilesQueue.front();
			ModifiedFilesQueue.pop();

			if (std::find(FilesToReload.begin(), FilesToReload.end(), FileName) == FilesToReload.end())
			{
				FilesToReload.push_back(FileName);
			}
		}
	}

	if (!FilesToReload.empty())
	{
		std::lock_guard<std::mutex> CompLock(ComponentMutex);
		for (const FString& FileName : FilesToReload)
		{
			for (ULuaScriptComponent* Component : ActiveComponents)
			{
				if (Component->GetScriptPath() == FileName)
				{
					Component->ReloadScript();
				}
			}
		}
	}
}

void FLuaScriptManager::WatchFunction()
{
	std::wstring WatchDir = FPaths::Combine(FPaths::ContentDir(), L"Scripts");
	HANDLE hDir = CreateFileW(
		WatchDir.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
	);

	if (hDir == INVALID_HANDLE_VALUE) return;

	BYTE Buffer[1024];
	DWORD BytesReturned;

	while (bIsRunning)
	{
		if (ReadDirectoryChangesW(
			hDir, Buffer, sizeof(Buffer), TRUE,
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
			&BytesReturned, NULL, NULL))
		{
			FILE_NOTIFY_INFORMATION* Offset = (FILE_NOTIFY_INFORMATION*)Buffer;
			do
			{
				std::wstring WideName(Offset->FileName, Offset->FileNameLength / sizeof(WCHAR));
				FString FileName = FPaths::ToUtf8(WideName);

				std::lock_guard<std::mutex> Lock(QueueMutex);
				ModifiedFilesQueue.push(FileName);

				if (Offset->NextEntryOffset == 0) break;
				Offset = (FILE_NOTIFY_INFORMATION*)((BYTE*)Offset + Offset->NextEntryOffset);
			} while (true);
		}
	}
	CloseHandle(hDir);
}

void FLuaScriptManager::RegisterComponent(ULuaScriptComponent* Component)
{
	std::lock_guard<std::mutex> Lock(ComponentMutex);
	ActiveComponents.push_back(Component);
}

void FLuaScriptManager::UnRegisterComponent(ULuaScriptComponent* Component)
{
	std::lock_guard<std::mutex> Lock(ComponentMutex);
	auto It = std::find(ActiveComponents.begin(), ActiveComponents.end(), Component);
	if (It != ActiveComponents.end())
	{
		ActiveComponents.erase(It);
	}
}

TArray<FString> FLuaScriptManager::GetAvailableScripts() const
{
	TArray<FString> Scripts;

	if (!std::filesystem::exists(SCRIPT_DIRECTORY))
	{
		return Scripts;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(SCRIPT_DIRECTORY))
	{
	    if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
	    {
			Scripts.push_back(Entry.path().filename().string());
	    }
	}
	return Scripts;
}