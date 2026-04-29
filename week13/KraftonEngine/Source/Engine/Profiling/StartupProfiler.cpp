#include "Profiling/StartupProfiler.h"
#include "Core/Log.h"

void FStartupProfiler::Finish()
{
	if (!bActive) return;
	bActive = false;

	LARGE_INTEGER Now;
	QueryPerformanceCounter(&Now);
	double TotalMs = static_cast<double>(Now.QuadPart - GlobalStart.QuadPart)
		/ static_cast<double>(Frequency.QuadPart) * 1000.0;

	UE_LOG("========== Startup Profile ==========");
	for (const FStartupEntry& E : Entries)
	{
		UE_LOG("  %-30s : %8.1f ms", E.Name, E.ElapsedMs);
	}
	UE_LOG("  %-30s : %8.1f ms", "Total", TotalMs);
	UE_LOG("======================================");
}
