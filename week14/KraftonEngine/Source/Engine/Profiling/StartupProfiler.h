#pragma once

#include "Core/CoreTypes.h"

#define NOMINMAX
#include <Windows.h>

// ============================================================
// FStartupProfiler — 엔진 시작 시 1회성 구간 시간 측정.
//
// RAII 매크로(SCOPE_STARTUP_STAT)로 측정, 결과는 엔진 로그에 출력.
// Finish() 호출 시 전체 요약 + Total을 로그에 남기고 비활성화.
// Release 빌드에서도 항상 동작한다 (시작 성능은 빌드 무관 관심사).
// ============================================================

struct FStartupEntry
{
	const char* Name = nullptr;
	double ElapsedMs = 0.0;
};

class FStartupProfiler
{
public:
	static FStartupProfiler& Get()
	{
		static FStartupProfiler Instance;
		return Instance;
	}

	bool IsActive() const { return bActive; }

	void Record(const char* Name, double ElapsedMs)
	{
		if (!bActive) return;
		Entries.push_back({ Name, ElapsedMs });
	}

	// 전체 결과를 로그에 출력하고 비활성화
	void Finish();

private:
	FStartupProfiler()
	{
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&GlobalStart);
	}

	bool bActive = true;
	LARGE_INTEGER Frequency;
	LARGE_INTEGER GlobalStart;
	TArray<FStartupEntry> Entries;

public:
	LARGE_INTEGER GetFrequency() const { return Frequency; }
};

// --- RAII Scoped Timer ---
class FStartupScopedTimer
{
public:
	FStartupScopedTimer(const char* InName) : Name(InName)
	{
		QueryPerformanceCounter(&Start);
	}

	~FStartupScopedTimer()
	{
		LARGE_INTEGER End;
		QueryPerformanceCounter(&End);
		double Ms = static_cast<double>(End.QuadPart - Start.QuadPart)
			/ static_cast<double>(FStartupProfiler::Get().GetFrequency().QuadPart) * 1000.0;
		FStartupProfiler::Get().Record(Name, Ms);
	}

private:
	const char* Name;
	LARGE_INTEGER Start;
};

// --- 매크로 ---
#define STARTUP_STAT_CONCAT2(a, b) a##b
#define STARTUP_STAT_CONCAT(a, b)  STARTUP_STAT_CONCAT2(a, b)
#define SCOPE_STARTUP_STAT(Name)   FStartupScopedTimer STARTUP_STAT_CONCAT(_StartupTimer_, __COUNTER__)(Name)
