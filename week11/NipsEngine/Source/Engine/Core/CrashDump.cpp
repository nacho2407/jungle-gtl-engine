#include "Engine/Core/CrashDump.h"
#include "Engine/Core/Paths.h"

#include <DbgHelp.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <cstdio>

#pragma comment(lib, "DbgHelp.lib")

LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* ExceptionInfo)
{
	FPaths::CreateDir(FPaths::DumpDir());

	// 타임스탬프 기반 파일명 생성
	WCHAR FileName[MAX_PATH];
	time_t Now = time(nullptr);
	tm LocalTime;
	localtime_s(&LocalTime, &Now);
	swprintf_s(FileName, L"Crash_%04d%02d%02d_%02d%02d%02d.dmp",
		LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
		LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);

	std::wstring DumpPath = FPaths::Combine(FPaths::DumpDir(), FileName);

	HANDLE File = CreateFileW(DumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (File != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION DumpInfo;
		DumpInfo.ThreadId = GetCurrentThreadId();
		DumpInfo.ExceptionPointers = ExceptionInfo;
		DumpInfo.ClientPointers = FALSE;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			File,
			MiniDumpWithDataSegs,
			&DumpInfo,
			nullptr,
			nullptr);

		CloseHandle(File);

		WCHAR Message[MAX_PATH + 64];
		swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s", DumpPath.c_str());
		MessageBoxW(nullptr, Message, L"Crash", MB_OK | MB_ICONERROR);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void WriteCrashLog(EXCEPTION_POINTERS* ExceptionInfo)
{
	FPaths::CreateDir(FPaths::DumpDir());

	WCHAR FileName[MAX_PATH];
	time_t Now = time(nullptr);
	tm LocalTime;
	localtime_s(&LocalTime, &Now);
	swprintf_s(FileName, L"CrashLog_%04d%02d%02d_%02d%02d%02d.txt",
		LocalTime.tm_year + 1900, LocalTime.tm_mon + 1, LocalTime.tm_mday,
		LocalTime.tm_hour, LocalTime.tm_min, LocalTime.tm_sec);

	std::filesystem::path LogPath(FPaths::Combine(FPaths::DumpDir(), FileName));
	std::ofstream LogFile(LogPath, std::ios::out | std::ios::app);
	if (!LogFile.is_open())
	{
		return;
	}

	HANDLE Process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	if (!SymInitialize(Process, nullptr, TRUE))
	{
		LogFile << "========================================\n";
		LogFile << "[Engine Crash Report]\n";
		LogFile << "Exception Code: 0x" << std::hex
			<< ExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
		LogFile << "Call Stack: (symbol init failed)\n";
		LogFile << "========================================\n\n";
		LogFile.close();
		return;
	}

	LogFile << "========================================\n";
	LogFile << "[Engine Crash Report]\n";
	LogFile << "Exception Code: 0x" << std::hex
		<< ExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
	LogFile << "Call Stack:\n";

	const int MaxFrames = 62;
	void* Stack[MaxFrames];
	WORD Frames = CaptureStackBackTrace(0, MaxFrames, Stack, nullptr);

	char SymbolBuffer[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO* Symbol = reinterpret_cast<SYMBOL_INFO*>(SymbolBuffer);
	Symbol->MaxNameLen = 255;
	Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	IMAGEHLP_LINE64 Line;
	Line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD Displacement = 0;

	for (WORD i = 0; i < Frames; ++i)
	{
		DWORD64 Address = reinterpret_cast<DWORD64>(Stack[i]);
		if (SymFromAddr(Process, Address, 0, Symbol))
		{
			if (SymGetLineFromAddr64(Process, Address, &Displacement, &Line))
			{
				LogFile << "[" << i << "] " << Symbol->Name << " (" << Line.FileName
					<< " : " << Line.LineNumber << ")\n";
			}
			else
			{
				LogFile << "[" << i << "] " << Symbol->Name << " (Address: 0x"
					<< std::hex << Address << std::dec << ")\n";
			}
		}
		else
		{
			LogFile << "[" << i << "] (Address: 0x" << std::hex << Address << std::dec
				<< ")\n";
		}
	}

	LogFile << "========================================\n\n";
	LogFile.close();

	SymCleanup(Process);
}

int ReportCrash(EXCEPTION_POINTERS* ExceptionInfo)
{
	WriteCrashDump(ExceptionInfo);
	WriteCrashLog(ExceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER;
}
