// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
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
#define STATS 1
#endif
#endif

// 슬라이딩 윈도우 크기
static constexpr uint32 STAT_WINDOW_SIZE = 60;

// --- Stat Entry (Snapshot용 — 읽기 전용) ---
struct FStatEntry
{
    const char* Name = nullptr;
    const char* Category = "Default";
    uint32 CallCount = 0;   // 현재 프레임 호출 횟수
    double TotalTime = 0.0; // 현재 프레임 합산 시간
    double AvgTime = 0.0;   // 최근 N프레임 평균
    double MaxTime = 0.0;   // 최근 N프레임 최대
    double MinTime = 0.0;   // 최근 N프레임 최소
    double LastTime = 0.0;  // 직전 프레임 시간
};

// --- 내부 누적용 Entry ---
struct FStatAccum
{
    const char* Name = nullptr;
    const char* Category = "Default";

    // 현재 프레임 누적
    uint32 FrameCallCount = 0;
    double FrameTotal = 0.0;

    // 슬라이딩 윈도우 (ring buffer)
    double Window[STAT_WINDOW_SIZE] = {};
    uint32 WindowHead = 0;  // 다음 쓸 위치
    uint32 WindowCount = 0; // 채워진 수 (최대 STAT_WINDOW_SIZE)
};

// FStatManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FStatManager : public TSingleton<FStatManager>
{
    friend class TSingleton<FStatManager>;

public:
    void RecordTime(const char* Name, double ElapsedSeconds, const char* Category = "Default");
    void TakeSnapshot();
    const TArray<FStatEntry>& GetSnapshot() const { return Snapshot; }
    LARGE_INTEGER GetFrequency() const { return Frequency; }

private:
    FStatManager();
    ~FStatManager() = default;

    TMap<const char*, FStatAccum> Stats;
    TArray<FStatEntry> Snapshot;
    LARGE_INTEGER Frequency;
};

// FScopedTimer는 엔진 영역의 핵심 동작을 담당합니다.
class FScopedTimer
{
public:
    FScopedTimer(const char* InName, const char* InCategory = "Default")
        : Name(InName), Category(InCategory)
    {
        QueryPerformanceCounter(&StartTime);
    }

    ~FScopedTimer()
    {
        LARGE_INTEGER EndTime;
        QueryPerformanceCounter(&EndTime);
        double Elapsed = static_cast<double>(EndTime.QuadPart - StartTime.QuadPart) / static_cast<double>(FStatManager::Get().GetFrequency().QuadPart);
        FStatManager::Get().RecordTime(Name, Elapsed, Category);
    }

private:
    const char* Name;
    const char* Category;
    LARGE_INTEGER StartTime;
};

// --- Draw Call Counter ---
struct FDrawCallStats
{
    static uint32 Count;
    static void Reset() { Count = 0; }
    static void Increment() { ++Count; }
    static uint32 Get() { return Count; }
};

// --- Light Culling GPU Stats ---
struct FLightCullStats
{
    static void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    static bool IsEnabled() { return bEnabled; }
    static void ResetSample()
    {
        LastGPUTimeMs = 0.0f;
        LastEvaluationCount = 0;
        bHasSample = false;
    }
    static void Record(float InGPUTimeMs, uint32 InEvaluationCount)
    {
        LastGPUTimeMs = InGPUTimeMs;
        LastEvaluationCount = InEvaluationCount;
        bHasSample = true;
    }
    static bool HasSample() { return bHasSample; }
    static float GetGPUTimeMs() { return LastGPUTimeMs; }
    static uint32 GetEvaluationCount() { return LastEvaluationCount; }

private:
    static bool bEnabled;
    static bool bHasSample;
    static float LastGPUTimeMs;
    static uint32 LastEvaluationCount;
};

// --- LOD Distribution Counter ---
#if STATS
// FLODStats는 엔진 처리에 필요한 데이터를 묶는 구조체입니다.
struct FLODStats
{
    static uint32 LODCount[4];
    static void Reset() { LODCount[0] = LODCount[1] = LODCount[2] = LODCount[3] = 0; }
    static void Record(uint32 LOD)
    {
        if (LOD < 4)
            ++LODCount[LOD];
    }
    static uint32 GetLOD0() { return LODCount[0]; }
    static uint32 GetLOD1() { return LODCount[1]; }
    static uint32 GetLOD2() { return LODCount[2]; }
    static uint32 GetLOD3() { return LODCount[3]; }
};
#define LOD_STATS_RESET() FLODStats::Reset()
#define LOD_STATS_RECORD(LOD) FLODStats::Record(LOD)
#else
#define LOD_STATS_RESET() ((void)0)
#define LOD_STATS_RECORD(LOD) ((void)0)
#endif

// --- SCOPE_STAT 매크로 ---
#if STATS
#define SCOPE_STAT_CONCAT2(a, b) a##b
#define SCOPE_STAT_CONCAT(a, b) SCOPE_STAT_CONCAT2(a, b)
#define SCOPE_STAT(Name) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name)
#define SCOPE_STAT_CAT(Name, Cat) FScopedTimer SCOPE_STAT_CONCAT(_ScopedTimer_, __COUNTER__)(Name, Cat)
#else
#define SCOPE_STAT(Name) ((void)0)
#define SCOPE_STAT_CAT(Name, Cat) ((void)0)
#endif
