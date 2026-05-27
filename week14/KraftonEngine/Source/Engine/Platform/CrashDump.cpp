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

	bool CopyIfExists(const std::filesystem::path& SourcePath, const std::filesystem::path& TargetPath)
	{
		std::error_code Error;
		if (!std::filesystem::exists(SourcePath, Error) || Error)
		{
			return false;
		}

		std::filesystem::copy_file(SourcePath, TargetPath, std::filesystem::copy_options::overwrite_existing, Error);
		return !Error;
	}

	std::filesystem::path GetExecutablePath()
	{
		WCHAR ExecutablePath[MAX_PATH] = {};
		const DWORD Length = GetModuleFileNameW(nullptr, ExecutablePath, MAX_PATH);
		if (Length == 0 || Length >= MAX_PATH)
		{
			return {};
		}

		return std::filesystem::path(ExecutablePath);
	}

	void WriteCrashCopyLog(
		const std::wstring& LocalDumpPath,
		const std::wstring& SharedDumpRoot,
		const std::wstring& SharedDumpPath,
		const std::wstring& FailureReason,
		bool bSucceeded)
	{
		try
		{
			std::filesystem::path LogPath(LocalDumpPath);
			LogPath.replace_extension(L".copy.log.txt");

			std::ofstream LogFile(LogPath, std::ios::out | std::ios::trunc);
			if (!LogFile.is_open())
			{
				return;
			}

			LogFile << "Crash dump shared copy result: " << (bSucceeded ? "success" : "failed") << "\n";
			LogFile << "RootDir: " << FPaths::ToUtf8(FPaths::RootDir()) << "\n";
			LogFile << "ProjectSettings: " << FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()) << "\n";
			LogFile << "SharedDumpRoot: " << FPaths::ToUtf8(SharedDumpRoot) << "\n";
			LogFile << "SharedDumpPath: " << FPaths::ToUtf8(SharedDumpPath) << "\n";
			LogFile << "FailureReason: " << FPaths::ToUtf8(FailureReason) << "\n";
		}
		catch (...)
		{
		}
	}

	bool TryCopyDumpToSharedFolder(
		const std::wstring& LocalDumpPath,
		const WCHAR* FileName,
		std::wstring& OutSharedPath,
		std::wstring& OutSharedDumpRoot,
		std::wstring& OutFailureReason)
	{
		std::wstring SharedCrashDumpRoot = GetCrashDumpShareDir();
		OutSharedDumpRoot = SharedCrashDumpRoot;
		if (SharedCrashDumpRoot.empty())
		{
			OutFailureReason = L"CrashDumpShareDir is empty. Check KRAFTON_CRASH_DUMP_DIR or Settings/ProjectSettings.ini.";
			return false;
		}

		try
		{
			std::filesystem::path SharedDir =
				std::filesystem::path(SharedCrashDumpRoot) /
				GetComputerNameForPath() /
				std::filesystem::path(FileName).stem();
			std::error_code Error;
			std::filesystem::create_directories(SharedDir, Error);
			if (Error)
			{
				OutFailureReason = L"Failed to create shared crash dump directory: " + FPaths::ToWide(Error.message());
				return false;
			}

			std::filesystem::path SharedPath = SharedDir / FileName;
			std::filesystem::copy_file(LocalDumpPath, SharedPath, std::filesystem::copy_options::overwrite_existing, Error);
			if (Error)
			{
				OutFailureReason = L"Failed to copy dump file to shared directory: " + FPaths::ToWide(Error.message());
				return false;
			}

			const std::filesystem::path ExecutablePath = GetExecutablePath();
			if (!ExecutablePath.empty())
			{
				CopyIfExists(ExecutablePath, SharedDir / ExecutablePath.filename());

				std::filesystem::path PdbPath = ExecutablePath;
				PdbPath.replace_extension(L".pdb");
				CopyIfExists(PdbPath, SharedDir / PdbPath.filename());
			}

			OutSharedPath = SharedPath.wstring();
			return true;
		}
		catch (...)
		{
			OutFailureReason = L"Unexpected exception while copying crash dump to shared directory.";
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
		std::wstring SharedDumpRoot;
		std::wstring SharedDumpFailureReason;
		const bool bSharedDumpCopied = TryCopyDumpToSharedFolder(
			DumpPath,
			FileName,
			SharedDumpPath,
			SharedDumpRoot,
			SharedDumpFailureReason);
		WriteCrashCopyLog(DumpPath, SharedDumpRoot, SharedDumpPath, SharedDumpFailureReason, bSharedDumpCopied);

		WCHAR Message[4096];
		if (bHasExceptionLocation)
		{
			swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s\n\n공유 폴더 복사: %s\n%s\n\nException location:\n%hs:%lu",
				DumpPath.c_str(),
				bSharedDumpCopied ? L"성공" : L"실패",
				bSharedDumpCopied ? SharedDumpPath.c_str() : SharedDumpFailureReason.c_str(),
				ExceptionLocation.File,
				ExceptionLocation.Line);
		}
		else
		{
			swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s\n\n공유 폴더 복사: %s\n%s",
				DumpPath.c_str(),
				bSharedDumpCopied ? L"성공" : L"실패",
				bSharedDumpCopied ? SharedDumpPath.c_str() : SharedDumpFailureReason.c_str());
		}
		MessageBoxW(nullptr, Message, L"Crash", MB_OK | MB_ICONERROR);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
