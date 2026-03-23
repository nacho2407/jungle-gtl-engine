#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

#define NOMINMAX
#include <Windows.h>
#include <cfloat>

// --- 빌드 설정 ---
#ifndef STATS
#if defined(_DEBUG) || defined(DEBUG)
#define STATS 1
#else
#define STATS 0
#endif
#endif

// --- Stat Entry ---
struct FStatEntry
{
	const char* Name = nullptr;
	uint32 CallCount = 0;
	double TotalTime = 0.0;		// seconds
	double MaxTime   = 0.0;
	double MinTime   = DBL_MAX;
	double LastTime  = 0.0;

	double GetAvgTime() const { return CallCount > 0 ? TotalTime / CallCount : 0.0; }
};

// --- Stat Manager (싱글턴) ---
class FStatManager : public TSingleton<FStatManager>
{
	friend class TSingleton<FStatManager>;

public:
	void RecordTime(const char* Name, double ElapsedSeconds);
	void TakeSnapshot();
	const TArray<FStatEntry>& GetSnapshot() const { return Snapshot; }
	LARGE_INTEGER GetFrequency() const { return Frequency; }

private:
	FStatManager();
	~FStatManager() = default;

	TMap<const char*, FStatEntry> Stats;
	TArray<FStatEntry> Snapshot;
	LARGE_INTEGER Frequency;
};

// --- Scoped Timer (RAII) ---
class FScopedTimer
{
public:
	FScopedTimer(const char* InName) : Name(InName)
	{
		QueryPerformanceCounter(&StartTime);
	}

	~FScopedTimer()
	{
		LARGE_INTEGER EndTime;
		QueryPerformanceCounter(&EndTime);
		double Elapsed = static_cast<double>(EndTime.QuadPart - StartTime.QuadPart)
			/ static_cast<double>(FStatManager::Get().GetFrequency().QuadPart);
		FStatManager::Get().RecordTime(Name, Elapsed);
	}

private:
	const char* Name;
	LARGE_INTEGER StartTime;
};

// --- SCOPE_STAT 매크로 ---
#if STATS
#define SCOPE_STAT_CONCAT2(a, b) a##b
#define SCOPE_STAT_CONCAT(a, b)  SCOPE_STAT_CONCAT2(a, b)
#define SCOPE_STAT(Name) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name)
#else
#define SCOPE_STAT(Name) ((void)0)
#endif
