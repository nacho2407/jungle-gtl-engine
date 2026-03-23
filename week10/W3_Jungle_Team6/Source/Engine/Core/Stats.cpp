#include "Core/Stats.h"

#include <algorithm>

FStatManager::FStatManager()
{
	QueryPerformanceFrequency(&Frequency);
}

void FStatManager::RecordTime(const char* Name, double ElapsedSeconds)
{
	auto it = Stats.find(Name);
	if (it == Stats.end())
	{
		FStatEntry Entry;
		Entry.Name = Name;
		Entry.CallCount = 1;
		Entry.TotalTime = ElapsedSeconds;
		Entry.MaxTime = ElapsedSeconds;
		Entry.MinTime = ElapsedSeconds;
		Entry.LastTime = ElapsedSeconds;
		Stats[Name] = Entry;
		return;
	}

	FStatEntry& Entry = it->second;
	Entry.CallCount++;
	Entry.TotalTime += ElapsedSeconds;
	Entry.MaxTime = (std::max)(Entry.MaxTime, ElapsedSeconds);
	Entry.MinTime = (std::min)(Entry.MinTime, ElapsedSeconds);
	Entry.LastTime = ElapsedSeconds;
}

void FStatManager::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(Stats.size());

	for (auto& [Key, Entry] : Stats)
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
