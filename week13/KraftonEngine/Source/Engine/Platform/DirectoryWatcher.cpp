#include "Engine/Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Core/Log.h"
#include <algorithm>

// ============================================================
// Initialize / Shutdown
// ============================================================
void FDirectoryWatcher::Initialize()
{
	if (bRunning) return;

	StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	bRunning = true;
	WatchThread = std::thread(&FDirectoryWatcher::WatchThreadFunc, this);
}

void FDirectoryWatcher::Shutdown()
{
	if (!bRunning) return;
	bRunning = false;

	if (StopEvent)
	{
		SetEvent(StopEvent);
	}

	if (WatchThread.joinable())
	{
		WatchThread.join();
	}

	// 감시 핸들 정리
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		for (auto& Entry : Watches)
		{
			if (Entry->DirHandle != INVALID_HANDLE_VALUE)
			{
				CancelIo(Entry->DirHandle);
				CloseHandle(Entry->Overlapped.hEvent);
				CloseHandle(Entry->DirHandle);
			}
		}
		Watches.clear();
		Subscriptions.clear();
	}

	if (StopEvent)
	{
		CloseHandle(StopEvent);
		StopEvent = nullptr;
	}
}

// ============================================================
// Watch / Unwatch
// ============================================================
FWatchID FDirectoryWatcher::Watch(const std::wstring& DirPath, const FString& PathPrefix)
{
	HANDLE hDir = CreateFileW(
		DirPath.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (hDir == INVALID_HANDLE_VALUE)
	{
		UE_LOG("[DirectoryWatcher] Failed to open directory: %s",
			FPaths::ToUtf8(DirPath).c_str());
		return 0;
	}

	auto Entry = std::make_unique<FWatchEntry>();
	Entry->DirPath = DirPath;
	Entry->PathPrefix = PathPrefix;
	Entry->DirHandle = hDir;
	Entry->Buffer.resize(FWatchEntry::kBufferSize);
	Entry->Overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

	FWatchID ID;
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		ID = NextWatchID++;
		Entry->ID = ID;
		BeginRead(*Entry);
		Watches.push_back(std::move(Entry));
	}

	UE_LOG("[DirectoryWatcher] Watching: %s (prefix=\"%s\", id=%u)",
		FPaths::ToUtf8(DirPath).c_str(), PathPrefix.c_str(), ID);
	return ID;
}

void FDirectoryWatcher::Unwatch(FWatchID ID)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	auto It = std::find_if(Watches.begin(), Watches.end(),
		[ID](const auto& E) { return E->ID == ID; });

	if (It != Watches.end())
	{
		CancelIo((*It)->DirHandle);
		CloseHandle((*It)->Overlapped.hEvent);
		CloseHandle((*It)->DirHandle);
		Watches.erase(It);
	}
}

// ============================================================
// Subscribe / Unsubscribe
// ============================================================
FSubscriptionID FDirectoryWatcher::Subscribe(FWatchID WatchID, FWatchCallback Callback)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	FSubscriptionID ID = NextSubID++;
	Subscriptions.push_back({ ID, WatchID, std::move(Callback) });
	return ID;
}

FSubscriptionID FDirectoryWatcher::SubscribeAll(FWatchCallback Callback)
{
	return Subscribe(0, std::move(Callback));
}

void FDirectoryWatcher::Unsubscribe(FSubscriptionID ID)
{
	std::lock_guard<std::mutex> Lock(Mutex);
	auto It = std::find_if(Subscriptions.begin(), Subscriptions.end(),
		[ID](const FSubscription& S) { return S.ID == ID; });
	if (It != Subscriptions.end())
	{
		Subscriptions.erase(It);
	}
}

// ============================================================
// ProcessChanges — 메인 스레드에서 호출
// 디바운스: 마지막 변경 후 kDebounceMs 경과해야 디스패치
// ============================================================
void FDirectoryWatcher::ProcessChanges()
{
	DWORD Now = GetTickCount();

	// 각 Watch별 변경 사항 수집 (lock 최소화)
	TArray<TPair<FWatchID, TSet<FString>>> Collected;

	{
		std::lock_guard<std::mutex> Lock(Mutex);
		for (auto& Entry : Watches)
		{
			if (!Entry->PendingChanges.empty()
				&& (Now - Entry->LastChangeTickMs) >= FWatchEntry::kDebounceMs)
			{
				TSet<FString> Changes;
				std::swap(Changes, Entry->PendingChanges);
				Collected.push_back({ Entry->ID, std::move(Changes) });
			}
		}
	}

	// 콜백 디스패치 (lock 밖에서)
	for (auto& [WatchID, Changes] : Collected)
	{
		// lock 안에서 구독 목록 복사 (콜백 중 구독 변경 가능)
		TArray<FSubscription> SubsCopy;
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			SubsCopy = Subscriptions;
		}

		for (const auto& Sub : SubsCopy)
		{
			if (Sub.WatchID == 0 || Sub.WatchID == WatchID)
			{
				Sub.Callback(Changes);
			}
		}
	}
}

// ============================================================
// BeginRead — OVERLAPPED ReadDirectoryChangesW 시작
// ============================================================
void FDirectoryWatcher::BeginRead(FWatchEntry& Entry)
{
	// OVERLAPPED 재초기화 (hEvent 보존, Internal/InternalHigh 클리어)
	HANDLE hEvent = Entry.Overlapped.hEvent;
	ZeroMemory(&Entry.Overlapped, sizeof(OVERLAPPED));
	Entry.Overlapped.hEvent = hEvent;
	ResetEvent(hEvent);

	BOOL Ok = ReadDirectoryChangesW(
		Entry.DirHandle,
		Entry.Buffer.data(),
		static_cast<DWORD>(Entry.Buffer.size()),
		TRUE,  // bWatchSubtree
		FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
		nullptr,
		&Entry.Overlapped,
		nullptr
	);

	if (!Ok)
	{
		DWORD Err = GetLastError();
		if (Err != ERROR_IO_PENDING)
		{
			UE_LOG("[DirectoryWatcher] ReadDirectoryChangesW failed: %lu", Err);
		}
	}
}

// ============================================================
// WatchThreadFunc — 감시 스레드 메인 루프
// ============================================================
void FDirectoryWatcher::WatchThreadFunc()
{
	while (bRunning)
	{
		// 대기 핸들 배열 구축
		TArray<HANDLE> Handles;
		Handles.push_back(StopEvent);

		TArray<FWatchEntry*> ActiveEntries;

		{
			std::lock_guard<std::mutex> Lock(Mutex);
			for (auto& Entry : Watches)
			{
				Handles.push_back(Entry->Overlapped.hEvent);
				ActiveEntries.push_back(Entry.get());
			}
		}

		if (Handles.size() <= 1)
		{
			// 감시 대상 없음 — 짧게 sleep 후 재확인
			Sleep(100);
			continue;
		}

		DWORD Result = WaitForMultipleObjects(
			static_cast<DWORD>(Handles.size()),
			Handles.data(),
			FALSE,
			500  // 500ms timeout — 새 Watch 추가 감지용
		);

		if (!bRunning) break;

		if (Result == WAIT_OBJECT_0)
		{
			// StopEvent 시그널
			break;
		}
		else if (Result == WAIT_TIMEOUT)
		{
			continue;
		}
		else if (Result >= WAIT_OBJECT_0 + 1 && Result < WAIT_OBJECT_0 + Handles.size())
		{
			DWORD Index = Result - WAIT_OBJECT_0 - 1; // -1: StopEvent 오프셋
			FWatchEntry* Entry = ActiveEntries[Index];

			DWORD BytesTransferred = 0;
			BOOL bGotResult = GetOverlappedResult(
				Entry->DirHandle, &Entry->Overlapped, &BytesTransferred, FALSE);

			if (bGotResult && BytesTransferred > 0)
			{
				// FILE_NOTIFY_INFORMATION 파싱
				BYTE* Base = Entry->Buffer.data();
				FILE_NOTIFY_INFORMATION* Info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Base);

				{
					std::lock_guard<std::mutex> Lock(Mutex);
					for (;;)
					{
						// wchar → UTF-8, 백슬래시 → 슬래시
						std::wstring FileName(Info->FileName, Info->FileNameLength / sizeof(WCHAR));
						for (auto& Ch : FileName)
						{
							if (Ch == L'\\') Ch = L'/';
						}
						FString NormalizedPath = Entry->PathPrefix + FPaths::ToUtf8(FileName);
						Entry->PendingChanges.insert(NormalizedPath);

						UE_LOG("[DirectoryWatcher] Detected: %s", NormalizedPath.c_str());

						if (Info->NextEntryOffset == 0) break;
						Info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
							reinterpret_cast<BYTE*>(Info) + Info->NextEntryOffset);
					}

					Entry->LastChangeTickMs = GetTickCount();
				}
			}

			// 다음 읽기 시작 (성공/실패 무관하게 항상 재등록)
			{
				std::lock_guard<std::mutex> Lock(Mutex);
				BeginRead(*Entry);
			}
		}
	}
}
