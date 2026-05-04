#include "Core/Logging/Log.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace
{
	FLog::SinkFn GLogSink = nullptr;
	FLog::DetailedSinkFn GDetailedLogSink = nullptr;
	std::wstring GLogFilePath;
	std::wstring GPerfLogFilePath;
	std::mutex GLogMutex;

	bool StartsWith(const char* Text, const char* Prefix)
	{
		return std::strncmp(Text, Prefix, std::strlen(Prefix)) == 0;
	}

	bool IsPerfTraceLogLine(const char* Text)
	{
		return StartsWith(Text, "[EditorFramePerf]") ||
			StartsWith(Text, "[PIEPerf]") ||
			StartsWith(Text, "[EditorPipelinePerf]") ||
			StartsWith(Text, "[EditorRenderPerf]") ||
			StartsWith(Text, "[GPUFramePerf]") ||
			StartsWith(Text, "[D3DPresentPerf]") ||
			StartsWith(Text, "[ScenePerf]") ||
			StartsWith(Text, "[RmlUiPerf]");
	}

	void ResetFile(const std::wstring& FilePath)
	{
		if (FilePath.empty())
		{
			return;
		}

		std::error_code Ec;
		std::filesystem::create_directories(std::filesystem::path(FilePath).parent_path(), Ec);
		std::ofstream File(FilePath, std::ios::trunc);
	}

	void AddLogInternal(ELogVerbosity Verbosity, const char* Format, va_list Args)
	{
		char Buffer[2048];
		vsnprintf(Buffer, sizeof(Buffer), Format, Args);

		FLog::SinkFn Sink = nullptr;
		FLog::DetailedSinkFn DetailedSink = nullptr;
		std::wstring FilePath;
		std::wstring PerfFilePath;
		{
			std::lock_guard<std::mutex> Lock(GLogMutex);
			Sink = GLogSink;
			DetailedSink = GDetailedLogSink;
			FilePath = GLogFilePath;
			PerfFilePath = GPerfLogFilePath;
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

		if (!PerfFilePath.empty() && IsPerfTraceLogLine(Buffer))
		{
			std::ofstream File(PerfFilePath, std::ios::app);
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
	ResetFile(GLogFilePath);
}

void FLog::SetPerfFileOutputPath(const std::wstring& InPath)
{
	std::lock_guard<std::mutex> Lock(GLogMutex);
	GPerfLogFilePath = InPath;
	ResetFile(GPerfLogFilePath);
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
