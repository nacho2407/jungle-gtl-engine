#include "TimerManager.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
	for (Timer* timer : Timers)
	{
		delete timer;
	}
	Timers.clear();
}

void TimerManager::Tick()
{
	for (Timer* timer : Timers)
	{
		timer->Tick();
	}
}

Timer* TimerManager::CreateGlobalTimer()
{
	if (GlobalTimer == nullptr)
	{
		GlobalTimer = new Timer();
		GlobalTimer->Initialize();
		Timers.push_back(GlobalTimer);

		return GlobalTimer;
	}
	return nullptr;
}

float TimerManager::GetGlobalDeltaTime() const
{
	return GlobalTimer ? GlobalTimer->GetDeltaTime() : 0.0f;
}

float TimerManager::GetGlobalTotalTime() const
{
	return GlobalTimer ? GlobalTimer->GetTotalTime() : 0.0f;
}

int TimerManager::GetGlobalFPS() const
{
	return GlobalTimer ? GlobalTimer->GetFPS() : 0;
}

Timer* TimerManager::CreateTimer()
{
	Timer* newTimer = new Timer();
	newTimer->Initialize();
	Timers.push_back(newTimer);
	return newTimer;
}

void TimerManager::ReleaseTimer(Timer* timer)
{
	auto it = std::find(Timers.begin(), Timers.end(), timer);
	if (it != Timers.end())
	{
		delete *it;
		Timers.erase(it);
	}
}
