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
	FLog::DetailedSinkFn GDetailedLogSink = nullptr;
	std::wstring GLogFilePath;
	std::mutex GLogMutex;

	void AddLogInternal(ELogVerbosity Verbosity, const char* Format, va_list Args)
	{
		char Buffer[2048];
		vsnprintf(Buffer, sizeof(Buffer), Format, Args);

		FLog::SinkFn Sink = nullptr;
		FLog::DetailedSinkFn DetailedSink = nullptr;
		std::wstring FilePath;
		{
			std::lock_guard<std::mutex> Lock(GLogMutex);
			Sink = GLogSink;
			DetailedSink = GDetailedLogSink;
			FilePath = GLogFilePath;
		}

		if (DetailedSink)
		{
			DetailedSink(Verbosity, Buffer);
		}
		else if (Sink)
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
}

void FLog::SetSink(SinkFn InSink)
{
	std::lock_guard<std::mutex> Lock(GLogMutex);
	GLogSink = InSink;
}

void FLog::SetDetailedSink(DetailedSinkFn InSink)
{
	std::lock_guard<std::mutex> Lock(GLogMutex);
	GDetailedLogSink = InSink;
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
	va_list Args;
	va_start(Args, Format);
	AddLogInternal(ELogVerbosity::Log, Format, Args);
	va_end(Args);
}

void FLog::AddWarning(const char* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	AddLogInternal(ELogVerbosity::Warning, Format, Args);
	va_end(Args);
}

void FLog::AddError(const char* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	AddLogInternal(ELogVerbosity::Error, Format, Args);
	va_end(Args);
}
