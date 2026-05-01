#pragma once

#include <string>

class FLog
{
public:
	using SinkFn = void (*)(const char* Message);

	static void SetSink(SinkFn InSink);
	static void SetFileOutputPath(const std::wstring& InPath);
	static void AddLog(const char* Format, ...);
};

#define UE_LOG(Format, ...) \
	FLog::AddLog(Format, ##__VA_ARGS__)
