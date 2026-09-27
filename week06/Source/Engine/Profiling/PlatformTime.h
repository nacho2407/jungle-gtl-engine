#pragma once
#include "Core/CoreTypes.h"
#include <Windows.h>

//5주차 경연을 위해 추가하는 picking time 측정용 코드입니다.
//기존 FScopedTimer와는 형식이 달라 발제에서 제공하는 코드를 우선 사용합니다.
//추후에 FScopedTimer와 통합할 예정입니다.

struct TStatId
{
};

class FWindowsPlatformTime
{
public:
	static void InitTiming()
	{
		if (!bInitialized)
		{
			bInitialized = true;

			double Frequency = static_cast<double>(GetFrequency());
			if (Frequency <= 0.0)
			{
				Frequency = 1.0;
			}

			GSecondsPerCycle = 1.0 / Frequency;
		}
	}

	static float GetSecondsPerCycle()
	{
		if (!bInitialized)
		{
			InitTiming();
		}

		return static_cast<float>(GSecondsPerCycle);
	}

	static uint64 GetFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return static_cast<uint64>(Frequency.QuadPart);
	}

	static double ToMilliseconds(uint64 CycleDiff)
	{
		return static_cast<double>(CycleDiff) * GetSecondsPerCycle() * 1000.0;
	}

	static uint64 Cycles64()
	{
		LARGE_INTEGER CycleCount;
		QueryPerformanceCounter(&CycleCount);
		return static_cast<uint64>(CycleCount.QuadPart);
	}

private:
	inline static double GSecondsPerCycle = 0.0;
	inline static bool bInitialized = false;
};

typedef FWindowsPlatformTime FPlatformTime;

class FScopeCycleCounter
{
public:
	FScopeCycleCounter(TStatId InStatId = {})
		: StartCycles(FPlatformTime::Cycles64())
		, UsedStatId(InStatId)
	{
	}

	~FScopeCycleCounter()
	{
		Finish();
	}

	uint64 Finish()
	{
		if (!bFinished)
		{
			const uint64 EndCycles = FPlatformTime::Cycles64();
			FinalCycleDiff = EndCycles - StartCycles;
			bFinished = true;
		}

		return FinalCycleDiff;
	}

private:
	uint64 StartCycles = 0;
	TStatId UsedStatId;
	bool bFinished = false;
	uint64 FinalCycleDiff = 0;
};
