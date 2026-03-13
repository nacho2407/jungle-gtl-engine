#pragma once
#include <windows.h>

class Timer
{
public:
	Timer();
	~Timer();

public:
	void Initialize();
	void Tick();

	void Reset();

	float GetDeltaTime() const { return DeltaTime; }
	float GetTotalTime() const { return TotalTime; }
	int GetFPS() const { return 1 / DeltaTime; }

private:
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;

	LARGE_INTEGER Frequency;

	LARGE_INTEGER StartTime = {};
	LARGE_INTEGER PrevTime = {};
	LARGE_INTEGER CurrTime = {};
};
