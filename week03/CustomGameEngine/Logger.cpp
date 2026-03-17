#include "Logger.h"
#include <cstdarg>

void Logger::Bind(LogCallback callback)
{
	Callback = std::move(callback);
}

void Logger::Unbind()
{
	Callback = nullptr;
}

void Logger::LogFormat(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	buf[sizeof(buf) - 1] = 0;
	va_end(args);

	if (Callback)
	{
		Callback(buf);
	}
}
