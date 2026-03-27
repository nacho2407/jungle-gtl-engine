#include "EngineLog.h"
#include <cstdarg>
#include <cstdio>

FEngineLog& FEngineLog::Get()
{
	static FEngineLog Instance;
	return Instance;
}

void FEngineLog::Log(const char* Format, ...)
{
	char Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	Buffer[sizeof(Buffer) - 1] = 0;
	va_end(Args);

	if (Callback)
	{
		Callback(Buffer);
	}
}

void FEngineLog::SetCallback(FLogCallback InCallback)
{
	Callback = InCallback;
}
