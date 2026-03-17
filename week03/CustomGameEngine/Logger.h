#pragma once

#include <functional>

class Logger
{
public:
	using LogCallback = std::function<void(const char*)>;

	static void Bind(LogCallback callback);
	static void Unbind();

	static void LogFormat(const char* fmt, ...);

private:
	inline static LogCallback Callback = nullptr;
};

#define UE_LOG(Format, ...) Logger::LogFormat(Format, ##__VA_ARGS__)
