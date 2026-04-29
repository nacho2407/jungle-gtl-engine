#include "Profiling/Stats.h"

#include <algorithm>

uint32 FDrawCallStats::Count = 0;

#if STATS
uint32 FLODStats::LODCount[4] = { 0, 0, 0, 0 };
#endif

FStatManager::FStatManager()
{
	QueryPerformanceFrequency(&Frequency);
}

void FStatManager::RecordTime(const char* Name, double ElapsedSeconds, const char* Category)
{
	FStatKey Key{ Name, Category };
	FStatAccum& Accum = Stats[Key];
	if (!Accum.Name)
	{
		Accum.Name = Name;
		Accum.Category = Category;
	}
	Accum.FrameCallCount++;
	Accum.FrameTotal += ElapsedSeconds;
}

void FStatManager::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(Stats.size());

	for (auto& [StatKey, Accum] : Stats)
	{
		// 현재 프레임 결과를 윈도우에 기록
		double FrameTime = Accum.FrameTotal;
		Accum.Window[Accum.WindowHead] = FrameTime;
		Accum.WindowHead = (Accum.WindowHead + 1) % STAT_WINDOW_SIZE;
		if (Accum.WindowCount < STAT_WINDOW_SIZE)
			Accum.WindowCount++;

		// 윈도우에서 Avg/Max/Min 계산
		double Sum = 0.0;
		double WMax = 0.0;
		double WMin = DBL_MAX;
		for (uint32 i = 0; i < Accum.WindowCount; ++i)
		{
			double Val = Accum.Window[i];
			Sum += Val;
			WMax = (std::max)(WMax, Val);
			WMin = (std::min)(WMin, Val);
		}

		FStatEntry Entry;
		Entry.Name = Accum.Name;
		Entry.Category = Accum.Category;
		Entry.CallCount = Accum.FrameCallCount;
		Entry.TotalTime = FrameTime;
		Entry.AvgTime = Accum.WindowCount > 0 ? Sum / Accum.WindowCount : 0.0;
		Entry.MaxTime = WMax;
		Entry.MinTime = WMin == DBL_MAX ? 0.0 : WMin;
		Entry.LastTime = FrameTime;
		Snapshot.push_back(Entry);

		// 현재 프레임 누적 리셋
		Accum.FrameCallCount = 0;
		Accum.FrameTotal = 0.0;
	}
}
