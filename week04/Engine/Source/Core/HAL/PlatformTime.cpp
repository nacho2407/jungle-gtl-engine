#include "Core/HAL/PlatformTime.h"

#if defined(_WIN32)

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

float FPlatformTime::Seconds()
{
    LARGE_INTEGER Counter{};
    ::QueryPerformanceCounter(&Counter);

    const LARGE_INTEGER Frequency = GetQpcFrequency();
    return static_cast<float>(Counter.QuadPart) / static_cast<float>(Frequency.QuadPart);
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

#else

double FPlatformTime::Seconds()
{
    using namespace std::chrono;
    const auto Now = steady_clock::now().time_since_epoch();
    return duration<double>(Now).count();
}

uint64 FPlatformTime::Cycles64()
{
    using namespace std::chrono;
    const auto Now = steady_clock::now().time_since_epoch();
    return static_cast<uint64>(duration_cast<nanoseconds>(Now).count());
}

void FPlatformTime::Sleep(float Seconds)
{
    if (Seconds <= 0.0f)
    {
        return;
    }

    std::this_thread::sleep_for(std::chrono::duration<float>(Seconds));
}

#endif