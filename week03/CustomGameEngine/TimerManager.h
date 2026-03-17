#pragma once

#include "EngineTypes.h"
#include "Singleton.h"
#include "Timer.h"

class TimerManager : public ISingleton<TimerManager>
{
	friend class ISingleton<TimerManager>;
private:
	TimerManager();
public:
	~TimerManager();

	void Tick();

	Timer* CreateGlobalTimer();
	Timer* GetGlobalTimer() const { return GlobalTimer; }

	float GetGlobalDeltaTime() const;
	float GetGlobalTotalTime() const;
	int GetGlobalFPS() const;

	Timer* CreateTimer();
	void ReleaseTimer(Timer* timer);

private:
	Timer* GlobalTimer = nullptr;
	TArray<Timer*> Timers;
};
