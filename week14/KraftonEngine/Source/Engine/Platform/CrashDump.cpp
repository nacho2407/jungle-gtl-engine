#include "Engine/Platform/CrashDump.h"
#include "Engine/Platform/Paths.h"

#include <DbgHelp.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "SimpleJSON/json.hpp"

#pragma comment(lib, "DbgHelp.lib")

namespace
{
	const wchar_t* CrashDumpShareDirEnv = L"KRAFTON_CRASH_DUMP_DIR";

	struct FSourceLocation
	{
		char File[MAX_PATH] = {};
		DWORD Line = 0;
	};

	std::wstring GetCrashDumpShareDirFromEnvironment()
	{
		DWORD RequiredSize = GetEnvironmentVariableW(CrashDumpShareDirEnv, nullptr, 0);
		if (RequiredSize == 0)
		{
			return {};
		}

		std::wstring Value(RequiredSize, L'\0');
		DWORD WrittenSize = GetEnvironmentVariableW(CrashDumpShareDirEnv, Value.data(), RequiredSize);
		if (WrittenSize == 0)
		{
			return {};
		}

		Value.resize(WrittenSize);
		return Value;
	}

	std::wstring GetCrashDumpShareDirFromProjectSettings()
	{
		std::ifstream File{ std::filesystem::path(FPaths::ProjectSettingsFilePath()) };
		if (!File.is_open())
		{
			return {};
		}

		std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		json::JSON Root = json::JSON::Load(Content);
		if (!Root.hasKey("Diagnostics"))
		{
			return {};
		}

		json::JSON Diagnostics = Root["Diagnostics"];
		if (!Diagnostics.hasKey("CrashDumpShareDir"))
		{
			return {};
		}

		return FPaths::ToWide(Diagnostics["CrashDumpShareDir"].ToString());
	}

	std::wstring GetCrashDumpShareDir()
	{
		std::wstring ShareDir = GetCrashDumpShareDirFromEnvironment();
		if (!ShareDir.empty())
		{
			return ShareDir;
		}

		return GetCrashDumpShareDirFromProjectSettings();
	}

	std::wstring GetComputerNameForPath()
	{
		WCHAR ComputerName[MAX_COMPUTERNAME_LENGTH + 1] = {};
		DWORD Size = MAX_COMPUTERNAME_LENGTH + 1;
		if (!GetComputerNameW(ComputerName, &Size))
		{
			return L"UnknownPC";
		}
		return ComputerName;
	}

	bool TryCopyDumpToSharedFolder(const std::wstring& LocalDumpPath, const WCHAR* FileName, std::wstring& OutSharedPath)
	{
		std::wstring SharedCrashDumpRoot = GetCrashDumpShareDir();
		if (SharedCrashDumpRoot.empty())
		{
			return false;
		}

		try
		{
			std::filesystem::path SharedDir = std::filesystem::path(SharedCrashDumpRoot) / GetComputerNameForPath();
			std::error_code Error;
			std::filesystem::create_directories(SharedDir, Error);
			if (Error)
			{
				return false;
			}

			std::filesystem::path SharedPath = SharedDir / FileName;
			std::filesystem::copy_file(LocalDumpPath, SharedPath, std::filesystem::copy_options::overwrite_existing, Error);
			if (Error)
			{
				return false;
			}

			OutSharedPath = SharedPath.wstring();
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool InitializeSymbols(HANDLE Process)
	{
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		if (!SymInitialize(Process, nullptr, TRUE) && GetLastError() != ERROR_INVALID_PARAMETER)
		{
			return false;
		}

		return true;
	}

	bool ResolveSourceLine(HANDLE Process, DWORD64 Address, FSourceLocation& OutLocation)
	{
		if (Address == 0)
		{
			return false;
		}

		IMAGEHLP_LINE64 LineInfo = {};
		LineInfo.SizeOfStruct = sizeof(LineInfo);

		DWORD Displacement = 0;
		if (!SymGetLineFromAddr64(Process, Address, &Displacement, &LineInfo))
		{
			return false;
		}

		strcpy_s(OutLocation.File, LineInfo.FileName);
		OutLocation.Line = LineInfo.LineNumber;
		return true;
	}

	bool ResolveSourceFromStack(EXCEPTION_POINTERS* ExceptionInfo, FSourceLocation& OutLocation)
	{
		if (!ExceptionInfo || !ExceptionInfo->ContextRecord)
		{
			return false;
		}

		HANDLE Process = GetCurrentProcess();
		HANDLE Thread = GetCurrentThread();

		CONTEXT Context = *ExceptionInfo->ContextRecord;
		STACKFRAME64 Frame = {};

#if defined(_M_X64)
		DWORD MachineType = IMAGE_FILE_MACHINE_AMD64;
		Frame.AddrPC.Offset = Context.Rip;
		Frame.AddrFrame.Offset = Context.Rbp;
		Frame.AddrStack.Offset = Context.Rsp;
#elif defined(_M_IX86)
		DWORD MachineType = IMAGE_FILE_MACHINE_I386;
		Frame.AddrPC.Offset = Context.Eip;
		Frame.AddrFrame.Offset = Context.Ebp;
		Frame.AddrStack.Offset = Context.Esp;
#else
		return false;
#endif

		Frame.AddrPC.Mode = AddrModeFlat;
		Frame.AddrFrame.Mode = AddrModeFlat;
		Frame.AddrStack.Mode = AddrModeFlat;

		for (int FrameIndex = 0; FrameIndex < 64; ++FrameIndex)
		{
			if (!StackWalk64(
				MachineType,
				Process,
				Thread,
				&Frame,
				&Context,
				nullptr,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				nullptr))
			{
				break;
			}

			if (ResolveSourceLine(Process, Frame.AddrPC.Offset, OutLocation))
			{
				return true;
			}
		}

		return false;
	}

	bool ResolveExceptionSource(EXCEPTION_POINTERS* ExceptionInfo, FSourceLocation& OutLocation)
	{
		if (!ExceptionInfo || !ExceptionInfo->ExceptionRecord)
		{
			return false;
		}

		HANDLE Process = GetCurrentProcess();
		if (!InitializeSymbols(Process))
		{
			return false;
		}

		const DWORD64 ExceptionAddress = reinterpret_cast<DWORD64>(ExceptionInfo->ExceptionRecord->ExceptionAddress);
		if (ResolveSourceLine(Process, ExceptionAddress, OutLocation))
		{
			return true;
		}

		return ResolveSourceFromStack(ExceptionInfo, OutLocation);
	}
}

__declspec(noinline) void CauseCrash()
{
	//crash 시 commit dump 확인
	ULONG_PTR ExceptionArguments[2] = { 1, 0 };
	RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 2, ExceptionArguments);
}

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

		FSourceLocation ExceptionLocation;
		const bool bHasExceptionLocation = ResolveExceptionSource(ExceptionInfo, ExceptionLocation);
		std::wstring SharedDumpPath;
		const bool bSharedDumpCopied = TryCopyDumpToSharedFolder(DumpPath, FileName, SharedDumpPath);

		WCHAR Message[4096];
		if (bHasExceptionLocation)
		{
			swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s\n\n공유 폴더 복사: %s\n%s\n\nException location:\n%hs:%lu",
				DumpPath.c_str(),
				bSharedDumpCopied ? L"성공" : L"실패",
				bSharedDumpCopied ? SharedDumpPath.c_str() : L"공유 폴더 접근 또는 쓰기 권한을 확인하세요.",
				ExceptionLocation.File,
				ExceptionLocation.Line);
		}
		else
		{
			swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s\n\n공유 폴더 복사: %s\n%s",
				DumpPath.c_str(),
				bSharedDumpCopied ? L"성공" : L"실패",
				bSharedDumpCopied ? SharedDumpPath.c_str() : L"공유 폴더 접근 또는 쓰기 권한을 확인하세요.");
		}
		MessageBoxW(nullptr, Message, L"Crash", MB_OK | MB_ICONERROR);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
