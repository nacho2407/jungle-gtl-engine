#include "Core/Log.h"
#include "Platform/Paths.h"

#include <Windows.h>
#include <cstdio>
#include <algorithm>

// ============================================================
// FDebugOutputDevice — VS 출력창 (OutputDebugStringA)
// ============================================================
class FDebugOutputDevice : public ILogOutputDevice
{
public:
	void Write(const char* Msg) override
	{
		OutputDebugStringA(Msg);
		OutputDebugStringA("\n");
	}
};

// ============================================================
// FFileOutputDevice — 파일 기록 (Logs/Engine.log)
// ============================================================
class FFileOutputDevice : public ILogOutputDevice
{
public:
	FFileOutputDevice()
	{
		std::wstring LogPath = FPaths::LogDir() + L"Engine.log";
		FPaths::CreateDir(FPaths::LogDir());
		_wfopen_s(&File, LogPath.c_str(), L"w");
	}

	~FFileOutputDevice() override
	{
		if (File)
		{
			fclose(File);
			File = nullptr;
		}
	}

	void Write(const char* Msg) override
	{
		if (!File) return;
		fprintf(File, "%s\n", Msg);
		fflush(File);
	}

private:
	FILE* File = nullptr;
};

// ============================================================
// FLogManager
// ============================================================

void FLogManager::Initialize()
{
	DebugOutputDevice = new FDebugOutputDevice();
	FileOutputDevice = new FFileOutputDevice();

	AddOutputDevice(DebugOutputDevice);
	AddOutputDevice(FileOutputDevice);
}

void FLogManager::Shutdown()
{
	std::lock_guard<std::mutex> Lock(Mutex);

	OutputDevices.clear();

	delete FileOutputDevice;
	FileOutputDevice = nullptr;

	delete DebugOutputDevice;
	DebugOutputDevice = nullptr;
}

void FLogManager::AddOutputDevice(ILogOutputDevice* Device)
{
	if (!Device) return;
	std::lock_guard<std::mutex> Lock(Mutex);
	OutputDevices.push_back(Device);
}

void FLogManager::RemoveOutputDevice(ILogOutputDevice* Device)
{
	if (!Device) return;
	std::lock_guard<std::mutex> Lock(Mutex);
	auto It = std::find(OutputDevices.begin(), OutputDevices.end(), Device);
	if (It != OutputDevices.end())
	{
		OutputDevices.erase(It);
	}
}

void FLogManager::Log(const char* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	LogV(Fmt, Args);
	va_end(Args);
}

void FLogManager::LogV(const char* Fmt, va_list Args)
{
	char Buffer[1024];
	vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);

	std::lock_guard<std::mutex> Lock(Mutex);
	for (ILogOutputDevice* Device : OutputDevices)
	{
		Device->Write(Buffer);
	}
}
