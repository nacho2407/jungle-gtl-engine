#include "Timer.h"

Timer::Timer()
{
}

Timer::~Timer()
{
}

void Timer::Initialize()
{
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartTime);
	PrevTime = StartTime;
}

void Timer::Tick()
{
	QueryPerformanceCounter(&CurrTime);

	DeltaTime = static_cast<float>(CurrTime.QuadPart - PrevTime.QuadPart) / static_cast<float>(Frequency.QuadPart);
	TotalTime = static_cast<float>(CurrTime.QuadPart - StartTime.QuadPart) / static_cast<float>(Frequency.QuadPart);

	PrevTime = CurrTime;
}

void Timer::Reset()
{
	QueryPerformanceCounter(&StartTime);
	PrevTime = StartTime;
	CurrTime = StartTime;
	DeltaTime = 0.0f;
	TotalTime = 0.0f;
}
