#include "LuaScriptManager.h"
#include "Component/LuaScriptComponent.h"
#include "Platform/Paths.h"

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
	DirectoryHandle = CreateFileW(
		WatchDir.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
	);

	if (DirectoryHandle == INVALID_HANDLE_VALUE) return;

	BYTE Buffer[1024];
	DWORD BytesReturned;

	while (bIsRunning)
	{
		if (ReadDirectoryChangesW(
			DirectoryHandle, Buffer, sizeof(Buffer), TRUE,
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
	CloseHandle(DirectoryHandle);
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
		std::lock_guard<std::mutex> CompLock(ComponentMutex);
		for (ULuaScriptComponent* Component : ActiveComponents)
		{
			if (Component->GetScriptPath() == FileName)
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

	std::lock_guard<std::mutex> CompLock(ComponentMutex);
	for (ULuaScriptComponent* Component : ActiveComponents)
	{
		if (Component->GetScriptPath() == OldName)
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
			if(WideFileName != L"Template.lua" && WideFileName != L"template.lua")
			{
				Scripts.push_back(FPaths::ToUtf8(WideFileName));
			}
	    }
	}
	return Scripts;
}