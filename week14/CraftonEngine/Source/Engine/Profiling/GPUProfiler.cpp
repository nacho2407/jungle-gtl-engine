#include "Profiling/GPUProfiler.h"

#include <algorithm>
#include <cfloat>

void FGPUProfiler::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	Device = InDevice;
	Context = InContext;

	D3D11_QUERY_DESC disjointDesc = {};
	disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

	D3D11_QUERY_DESC timestampDesc = {};
	timestampDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (uint32 f = 0; f < FRAME_COUNT; ++f)
	{
		Device->CreateQuery(&disjointDesc, &Frames[f].DisjointQuery);
		Frames[f].UsedCount = 0;
		Frames[f].bActive = false;

		for (uint32 i = 0; i < MAX_TIMESTAMPS; ++i)
		{
			Device->CreateQuery(&timestampDesc, &Frames[f].Timestamps[i].BeginQuery);
			Device->CreateQuery(&timestampDesc, &Frames[f].Timestamps[i].EndQuery);
			Frames[f].Timestamps[i].Name = nullptr;
		}
	}

	bInitialized = true;
}

void FGPUProfiler::Shutdown()
{
	if (!bInitialized) return;

	for (uint32 f = 0; f < FRAME_COUNT; ++f)
	{
		if (Frames[f].DisjointQuery) { Frames[f].DisjointQuery->Release(); Frames[f].DisjointQuery = nullptr; }

		for (uint32 i = 0; i < MAX_TIMESTAMPS; ++i)
		{
			if (Frames[f].Timestamps[i].BeginQuery) { Frames[f].Timestamps[i].BeginQuery->Release(); }
			if (Frames[f].Timestamps[i].EndQuery)   { Frames[f].Timestamps[i].EndQuery->Release(); }
			Frames[f].Timestamps[i].BeginQuery = nullptr;
			Frames[f].Timestamps[i].EndQuery = nullptr;
		}
	}

	bInitialized = false;
}

void FGPUProfiler::BeginFrame()
{
	if (!bInitialized) return;

	// 이전 프레임 결과 수집
	CollectPreviousFrame();

	// 현재 프레임 시작
	FFrameData& Write = Frames[WriteIndex];

	// 이 슬롯의 이전 결과가 아직 소비되지 않았다면 직접 소비 시도
	if (Write.bActive)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Dummy;
		if (Context->GetData(Write.DisjointQuery, &Dummy, sizeof(Dummy), 0) != S_OK)
		{
			// GPU가 아직 완료하지 않음 — 이 프레임은 프로파일링 건너뜀
			bFrameActive = false;
			return;
		}
		Write.bActive = false;
	}

	bFrameActive = true;
	Write.UsedCount = 0;
	Write.bActive = true;
	Context->Begin(Write.DisjointQuery);
}

void FGPUProfiler::EndFrame()
{
	if (!bInitialized || !bFrameActive) return;

	Context->End(Frames[WriteIndex].DisjointQuery);

	// 프레임 스왑
	WriteIndex = (WriteIndex + 1) % FRAME_COUNT;
}

uint32 FGPUProfiler::BeginTimestamp(const char* Name)
{
	if (!bInitialized || !bFrameActive) return UINT32_MAX;

	FFrameData& Write = Frames[WriteIndex];
	if (Write.UsedCount >= MAX_TIMESTAMPS) return UINT32_MAX;

	uint32 Idx = Write.UsedCount++;
	Write.Timestamps[Idx].Name = Name;
	Context->End(Write.Timestamps[Idx].BeginQuery);  // Timestamp은 End()로 기록
	return Idx;
}

void FGPUProfiler::EndTimestamp(uint32 Index)
{
	if (!bInitialized || !bFrameActive || Index == UINT32_MAX) return;

	FFrameData& Write = Frames[WriteIndex];
	if (Index >= Write.UsedCount) return;

	Context->End(Write.Timestamps[Index].EndQuery);
}

void FGPUProfiler::CollectPreviousFrame()
{
	uint32 ReadIndex = (WriteIndex + 1) % FRAME_COUNT;
	FFrameData& Read = Frames[ReadIndex];

	if (!Read.bActive) return;  // 수집할 데이터 없음

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
	HRESULT hr = Context->GetData(Read.DisjointQuery, &disjointData, sizeof(disjointData), 0);
	if (hr != S_OK) return;  // 아직 준비 안 됨 — 다음 프레임에 재시도

	Read.bActive = false;  // 결과 소비 완료

	if (disjointData.Disjoint || Read.UsedCount == 0)
	{
		return;
	}

	double InvFrequency = 1000.0 / static_cast<double>(disjointData.Frequency); // ms 단위

	for (uint32 i = 0; i < Read.UsedCount; ++i)
	{
		UINT64 tsBegin = 0, tsEnd = 0;
		if (Context->GetData(Read.Timestamps[i].BeginQuery, &tsBegin, sizeof(UINT64), 0) != S_OK) continue;
		if (Context->GetData(Read.Timestamps[i].EndQuery, &tsEnd, sizeof(UINT64), 0) != S_OK) continue;

		double ElapsedMs = static_cast<double>(tsEnd - tsBegin) * InvFrequency;
		double ElapsedSec = ElapsedMs * 0.001;

		const char* Name = Read.Timestamps[i].Name;
		auto it = GPUStats.find(Name);
		if (it == GPUStats.end())
		{
			FStatEntry Entry;
			Entry.Name = Name;
			Entry.CallCount = 1;
			Entry.TotalTime = ElapsedSec;
			Entry.MaxTime = ElapsedSec;
			Entry.MinTime = ElapsedSec;
			Entry.LastTime = ElapsedSec;
			GPUStats[Name] = Entry;
		}
		else
		{
			FStatEntry& Entry = it->second;
			Entry.CallCount++;
			Entry.TotalTime += ElapsedSec;
			Entry.MaxTime = (std::max)(Entry.MaxTime, ElapsedSec);
			Entry.MinTime = (std::min)(Entry.MinTime, ElapsedSec);
			Entry.LastTime = ElapsedSec;
		}
	}
}

void FGPUProfiler::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(GPUStats.size());

	for (auto& [Key, Entry] : GPUStats)
	{
		Snapshot.push_back(Entry);

		// Reset for next frame
		Entry.CallCount = 0;
		Entry.TotalTime = 0.0;
		Entry.MaxTime = 0.0;
		Entry.MinTime = DBL_MAX;
		Entry.LastTime = 0.0;
	}
}
