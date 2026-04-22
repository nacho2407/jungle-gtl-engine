#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include <cstdarg>
#include <mutex>

// ============================================================
// ILogOutputDevice — 로그 출력 대상 인터페이스
// ============================================================
class ILogOutputDevice
{
public:
	virtual ~ILogOutputDevice() = default;
	virtual void Write(const char* FormattedMessage) = 0;
};

// ============================================================
// FLogManager — 중앙 로그 관리 싱글턴
//
// 문자열을 1회 포맷한 뒤 등록된 모든 OutputDevice에 디스패치한다.
// 기본 디바이스: VS 출력창(OutputDebugStringA), 파일(Logs/Engine.log)
// 에디터 콘솔은 Editor 레이어에서 AddOutputDevice로 등록한다.
// ============================================================
class FLogManager : public TSingleton<FLogManager>
{
	friend class TSingleton<FLogManager>;

public:
	void Initialize();
	void Shutdown();

	void AddOutputDevice(ILogOutputDevice* Device);
	void RemoveOutputDevice(ILogOutputDevice* Device);

	void Log(const char* Fmt, ...);
	void LogV(const char* Fmt, va_list Args);

private:
	FLogManager() = default;

	std::mutex Mutex;
	TArray<ILogOutputDevice*> OutputDevices;

	// 내장 디바이스 (Initialize에서 생성, Shutdown에서 해제)
	ILogOutputDevice* DebugOutputDevice = nullptr;
	ILogOutputDevice* FileOutputDevice = nullptr;
};

// ============================================================
// UE_LOG 매크로 — Engine 레이어에서 정의
// ============================================================
#define UE_LOG(Format, ...) \
	FLogManager::Get().Log(Format, ##__VA_ARGS__)
