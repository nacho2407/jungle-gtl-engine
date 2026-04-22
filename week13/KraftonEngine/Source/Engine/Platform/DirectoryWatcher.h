#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

#include <Windows.h>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

// ============================================================
// FDirectoryWatcher — 범용 파일 감시 싱글턴
//
// ReadDirectoryChangesW 기반. 별도 감시 스레드에서 변경 감지 후
// ProcessChanges()에서 메인 스레드 콜백 디스패치.
// ============================================================

using FWatchID = uint32;
using FSubscriptionID = uint32;
using FWatchCallback = std::function<void(const TSet<FString>&)>;

struct FWatchEntry
{
	FWatchID        ID = 0;
	std::wstring    DirPath;       // 감시 디렉토리 절대 경로
	FString         PathPrefix;    // 구독자에게 전달할 접두사 (e.g. "Shaders/")
	HANDLE          DirHandle = INVALID_HANDLE_VALUE;
	OVERLAPPED      Overlapped = {};
	TArray<BYTE>    Buffer;        // FILE_NOTIFY_INFORMATION 수신 버퍼
	TSet<FString>   PendingChanges; // 스레드→메인 전달용
	DWORD           LastChangeTickMs = 0; // 마지막 변경 감지 시각 (GetTickCount)

	static constexpr DWORD kBufferSize = 4096;
	static constexpr DWORD kDebounceMs = 200; // 파일 저장 완료 대기
};

struct FSubscription
{
	FSubscriptionID ID = 0;
	FWatchID        WatchID = 0;   // 0 = 전체 구독
	FWatchCallback  Callback;
};

class FDirectoryWatcher : public TSingleton<FDirectoryWatcher>
{
	friend class TSingleton<FDirectoryWatcher>;

public:
	void Initialize();
	void Shutdown();

	FWatchID        Watch(const std::wstring& DirPath, const FString& PathPrefix);
	void            Unwatch(FWatchID ID);

	FSubscriptionID Subscribe(FWatchID WatchID, FWatchCallback Callback);
	FSubscriptionID SubscribeAll(FWatchCallback Callback);
	void            Unsubscribe(FSubscriptionID ID);

	// 메인 스레드에서 매 프레임 호출. 변경된 파일을 구독자에게 디스패치.
	void ProcessChanges();

private:
	FDirectoryWatcher() = default;

	void WatchThreadFunc();
	void BeginRead(FWatchEntry& Entry);

	std::mutex                          Mutex;
	TArray<std::unique_ptr<FWatchEntry>> Watches;
	TArray<FSubscription>               Subscriptions;

	std::thread                         WatchThread;
	HANDLE                              StopEvent = nullptr;
	std::atomic<bool>                   bRunning{false};

	FWatchID        NextWatchID = 1;
	FSubscriptionID NextSubID = 1;
};
