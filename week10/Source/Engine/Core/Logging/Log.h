#pragma once

#include <string>

enum class ELogVerbosity
{
	Log,
	Warning,
	Error,
};

class FLog
{
public:
	using SinkFn = void (*)(const char* Message);
	using DetailedSinkFn = void (*)(ELogVerbosity Verbosity, const char* Message);

	static void SetSink(SinkFn InSink);
	static void SetDetailedSink(DetailedSinkFn InSink);
	static void SetFileOutputPath(const std::wstring& InPath);
	static void SetPerfFileOutputPath(const std::wstring& InPath);
	static void AddLog(const char* Format, ...);
	static void AddWarning(const char* Format, ...);
	static void AddError(const char* Format, ...);
};

#define UE_LOG(Format, ...) \
	FLog::AddLog(Format, ##__VA_ARGS__)

#define UE_LOG_WARNING(Format, ...) \
	FLog::AddWarning(Format, ##__VA_ARGS__)

#define UE_LOG_ERROR(Format, ...) \
	FLog::AddError(Format, ##__VA_ARGS__)
