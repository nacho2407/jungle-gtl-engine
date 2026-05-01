#include "Core/Logging/Log.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace
{
	FLog::SinkFn GLogSink = nullptr;
	std::wstring GLogFilePath;
	std::mutex GLogMutex;
}

void FLog::SetSink(SinkFn InSink)
{
	std::lock_guard<std::mutex> Lock(GLogMutex);
	GLogSink = InSink;
}

void FLog::SetFileOutputPath(const std::wstring& InPath)
{
	std::lock_guard<std::mutex> Lock(GLogMutex);
	GLogFilePath = InPath;
	if (!GLogFilePath.empty())
	{
		std::error_code Ec;
		std::filesystem::create_directories(std::filesystem::path(GLogFilePath).parent_path(), Ec);
		std::ofstream File(GLogFilePath, std::ios::trunc);
	}
}

void FLog::AddLog(const char* Format, ...)
{
	char Buffer[2048];
	va_list Args;
	va_start(Args, Format);
	vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);

	SinkFn Sink = nullptr;
	std::wstring FilePath;
	{
		std::lock_guard<std::mutex> Lock(GLogMutex);
		Sink = GLogSink;
		FilePath = GLogFilePath;
	}

	if (Sink)
	{
		Sink(Buffer);
	}

	OutputDebugStringA(Buffer);
	OutputDebugStringA("\n");

	if (!FilePath.empty())
	{
		std::ofstream File(FilePath, std::ios::app);
		if (File.is_open())
		{
			File << Buffer << '\n';
		}
	}
}
