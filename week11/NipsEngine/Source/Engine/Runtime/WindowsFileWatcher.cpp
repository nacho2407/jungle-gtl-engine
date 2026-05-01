#include "WindowsFileWatcher.h"

#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Core/Logging/Log.h"
#include "Render/Resource/Shader.h"

#include <chrono>

bool FWindowsFileWatcher::Initialize(const FWString& InDirectory, bool bInRecursive)
{
	WatchedDirectory = InDirectory;
	bRecursive = bInRecursive;
	DirectoryHandle = CreateFileW(
		FPaths::ToWide(FPaths::ToUtf8(WatchedDirectory)).c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		nullptr);

	if (DirectoryHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	EventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!EventHandle)
	{
		CloseHandle(DirectoryHandle);
		return false;
	}

	Buffer.resize((1024 * 64) / sizeof(uint32));

	return BeginRead();
}

void FWindowsFileWatcher::Tick()
{
	ProcessPendingChanges();

	if (!bPendingRead || EventHandle == nullptr)
	{
		return;
	}

	const DWORD WaitResult = WaitForSingleObject(EventHandle, 0);
	if (WaitResult == WAIT_TIMEOUT)
	{
		return;
	}

	DWORD BytesTransferred = 0;
	if (!GetOverlappedResult(DirectoryHandle, &Overlapped, &BytesTransferred, FALSE))
	{
		UE_LOG("[FileWatcher] Read failed");
		bPendingRead = false;
		ResetEvent(EventHandle);
		BeginRead();
		return;
	}

	bPendingRead = false;
	ResetEvent(EventHandle);

	if (BytesTransferred > 0)
	{
		ProcessNotifications(BytesTransferred);
		BeginRead();
		return;
	}

	BeginRead();
}

void FWindowsFileWatcher::Shutdown()
{
	if (DirectoryHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(DirectoryHandle);
		DirectoryHandle = INVALID_HANDLE_VALUE;
	}
	if (EventHandle)
	{
		CloseHandle(EventHandle);
		EventHandle = nullptr;
	}
}

bool FWindowsFileWatcher::BeginRead()
{
	ZeroMemory(&Overlapped, sizeof(Overlapped));
	Overlapped.hEvent = EventHandle;
	ResetEvent(EventHandle);

	constexpr DWORD NotifyFilter =
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
		FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

	DWORD BytesIgnored = 0;
	const BOOL bOk = ReadDirectoryChangesW(
		DirectoryHandle,
		Buffer.data(),
		static_cast<DWORD>(Buffer.size()),
		bRecursive,
		NotifyFilter,
		&BytesIgnored,
		&Overlapped,
		nullptr);

	if (!bOk)
	{
		return false;
	}

	bPendingRead = true;
	return true;
}

void FWindowsFileWatcher::ProcessNotifications(uint32 BytesTransferred) 
{
	const uint8* Cursor = Buffer.data();
	const uint8* End = Buffer.data() + BytesTransferred;

	while (Cursor < End)
	{
		const FILE_NOTIFY_INFORMATION* Info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(Cursor);

		const int32 CharCount = static_cast<int32>(Info->FileNameLength / sizeof(WCHAR));
		const FWString FileName(Info->FileName, Info->FileName + CharCount);
		const std::filesystem::path FullPath = std::filesystem::path(WatchedDirectory) / FileName;

		if (Info->Action == FILE_ACTION_MODIFIED ||
			Info->Action == FILE_ACTION_ADDED ||
			Info->Action == FILE_ACTION_RENAMED_NEW_NAME)
		{
			OnFileEvent(FullPath);
		}

		if (Info->NextEntryOffset == 0)
		{
			break;
		}

		Cursor += Info->NextEntryOffset;
	}
}

bool IsRelevantShaderFile(const std::filesystem::path& Path)
{
	const std::filesystem::path Extension = Path.extension();
	return (Extension == L".hlsl" || Extension == L".hlsli" || Extension == L".h");
}


void FWindowsFileWatcher::ProcessPendingChanges()
{
	const auto Now = FClock::now();

	for (auto It = PendingChanges.begin(); It != PendingChanges.end(); )
	{
		if (Now - It->second.LastEventTime < std::chrono::milliseconds(300))
		{
			++It;
			continue;
		}

		std::filesystem::path Path(It->first);
		if (std::filesystem::exists(Path) && IsRelevantShaderFile(Path))
		{
			UE_LOG("[ShaderHotReload] Stable change detected: %s", FPaths::ToUtf8(Path.wstring()).c_str());

			FString RelativePath = FPaths::ToUtf8(FPaths::ToRelative(Path.wstring()));
			FResourceManager::Get().ReloadShader(RelativePath);
		}

		It = PendingChanges.erase(It);
	}
}

const char* FWindowsFileWatcher::ActionToString(uint32 Action) const
{
	switch (Action)
	{
	case FILE_ACTION_ADDED: return "Added";
	case FILE_ACTION_REMOVED: return "Removed";
	case FILE_ACTION_MODIFIED: return "Modified";
	case FILE_ACTION_RENAMED_OLD_NAME: return "Renamed(Old)";
	case FILE_ACTION_RENAMED_NEW_NAME: return "Renamed(New)";
	default: return "Unknown";
	}
}

void FWindowsFileWatcher::OnFileEvent(const std::filesystem::path& Path)
{
	PendingChanges[Path.wstring()].LastEventTime = FClock::now();
}
