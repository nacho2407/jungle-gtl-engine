#include "PlatformTime.h"

#include <Windows.h>

namespace
{
	inline LARGE_INTEGER GetQpcFrequency()
	{
		static LARGE_INTEGER Frequency = []()
			{
				LARGE_INTEGER Value{};
				::QueryPerformanceFrequency(&Value);
				return Value;
			}();
		return Frequency;
	}
} // namespace

double FPlatformTime::Seconds()
{
	LARGE_INTEGER Counter{};
	::QueryPerformanceCounter(&Counter);

	const LARGE_INTEGER Frequency = GetQpcFrequency();
	return static_cast<double>(Counter.QuadPart) / static_cast<double>(Frequency.QuadPart);
}

uint64 FPlatformTime::Cycles64()
{
	LARGE_INTEGER Counter{};
	::QueryPerformanceCounter(&Counter);
	return static_cast<uint64>(Counter.QuadPart);
}

void FPlatformTime::Sleep(float Seconds)
{
	if (Seconds < 0.0f)
	{
		return;
	}

	const DWORD Milliseconds = static_cast<DWORD>(Seconds * 1000.0f);
	::Sleep(Milliseconds);
}