#pragma once

#include "Core/CoreTypes.h"

struct FPlatformTime
{
	static double Seconds();
	static uint64 Cycles64();
	static void Sleep(float Seconds);
};

