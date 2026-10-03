#pragma once
#include "Core/CoreMinimal.h"
#include <windows.h>

class FWindowsPlatformTime
{
public:
	static double GSecondsPerCycle; 
	static bool bInitialized; 

	static void InitTiming();
	static float GetSecondsPerCycle();

	static uint64 GetFrequency();

	static double ToMilliseconds(uint64 CycleDiff);

	static uint64 Cycles64();
};