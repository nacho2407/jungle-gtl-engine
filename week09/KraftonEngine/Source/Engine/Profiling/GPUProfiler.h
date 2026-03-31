#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Profiling/Stats.h"

#include <d3d11.h>

// --- GPU Timestamp Profiler ---
// DX11 Timestamp Query 기반 GPU 시간 측정.
// 더블 버퍼링: Frame N에서 기록, Frame N+1에서 결과 읽기.
//
// 사용 흐름:
//   1) Initialize()  — Device/Context 연결, Query 풀 생성
//   2) BeginFrame()  — 이전 프레임 결과 수집 + 새 프레임 Disjoint 시작
//   3) GPU_SCOPE_STAT("Name") — 측정 구간 삽입 (RAII)
//   4) EndFrame()    — Disjoint 종료 + 프레임 스왑
//   5) Shutdown()    — Query 해제

class FGPUProfiler : public TSingleton<FGPUProfiler>
{
	friend class TSingleton<FGPUProfiler>;

public:
	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	uint32 BeginTimestamp(const char* Name);
	void EndTimestamp(uint32 Index);

	void TakeSnapshot();
	const TArray<FStatEntry>& GetGPUSnapshot() const { return Snapshot; }

private:
	FGPUProfiler() = default;
	~FGPUProfiler() = default;

	static const uint32 MAX_TIMESTAMPS = 64;
	static const uint32 FRAME_COUNT = 3;

	struct FTimestampPair
	{
		ID3D11Query* BeginQuery = nullptr;
		ID3D11Query* EndQuery = nullptr;
		const char* Name = nullptr;
	};

	struct FFrameData
	{
		ID3D11Query* DisjointQuery = nullptr;
		FTimestampPair Timestamps[MAX_TIMESTAMPS];
		uint32 UsedCount = 0;
		bool bActive = false;	// Begin()~GetData() 사이이면 true
	};

	FFrameData Frames[FRAME_COUNT];
	uint32 WriteIndex = 0;

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;
	bool bInitialized = false;
	bool bFrameActive = false;	// 현재 프레임에서 Begin()이 호출됐는지

	void CollectPreviousFrame();

	TMap<const char*, FStatEntry> GPUStats;
	TArray<FStatEntry> Snapshot;
};

// --- GPU Scoped Timer (RAII) ---
class FGPUScopedTimer
{
public:
	FGPUScopedTimer(const char* InName)
		: Index(FGPUProfiler::Get().BeginTimestamp(InName)) {}

	~FGPUScopedTimer()
	{
		FGPUProfiler::Get().EndTimestamp(Index);
	}

private:
	uint32 Index;
};

// --- GPU_SCOPE_STAT 매크로 ---
#if STATS
#define GPU_SCOPE_STAT(Name) FGPUScopedTimer SCOPE_STAT_CONCAT(_GPUScopedTimer_, __COUNTER__)(Name)
#else
#define GPU_SCOPE_STAT(Name) ((void)0)
#endif
