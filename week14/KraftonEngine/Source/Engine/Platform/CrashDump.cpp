#include "Engine/Platform/CrashDump.h"
#include "Engine/Platform/Paths.h"

#include <DbgHelp.h>
#include <cstddef>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include "SimpleJSON/json.hpp"

#pragma comment(lib, "DbgHelp.lib")

namespace
{
	const wchar_t* CrashDumpShareDirEnv = L"KRAFTON_CRASH_DUMP_DIR";

	struct FSourceLocation
	{
		wchar_t File[MAX_PATH] = {};
		DWORD Line = 0;
	};

	struct FPdbReference
	{
		std::wstring FileName;
		std::wstring DecimalStoreIndex;
		std::wstring HexStoreIndex;
	};

	struct FScopedHandle
	{
		HANDLE Handle = INVALID_HANDLE_VALUE;

		~FScopedHandle()
		{
			if (Handle != INVALID_HANDLE_VALUE && Handle != nullptr)
			{
				CloseHandle(Handle);
			}
		}

		bool IsValid() const
		{
			return Handle != INVALID_HANDLE_VALUE && Handle != nullptr;
		}
	};

	struct FCodeViewRsdsHeader
	{
		DWORD Signature = 0;
		GUID Guid = {};
		DWORD Age = 0;
		char PdbPath[1] = {};
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

	bool IsRangeInsideFile(uint64_t Offset, uint64_t Size, uint64_t FileSize)
	{
		return Offset <= FileSize && Size <= FileSize - Offset;
	}

	bool TryRvaToFileOffset(DWORD Rva, const IMAGE_NT_HEADERS* NtHeaders, DWORD& OutOffset)
	{
		if (Rva < NtHeaders->OptionalHeader.SizeOfHeaders)
		{
			OutOffset = Rva;
			return true;
		}

		const IMAGE_SECTION_HEADER* Section = IMAGE_FIRST_SECTION(NtHeaders);
		for (WORD Index = 0; Index < NtHeaders->FileHeader.NumberOfSections; ++Index, ++Section)
		{
			const uint64_t SectionStart = Section->VirtualAddress;
			const uint64_t SectionSize =
				Section->Misc.VirtualSize > Section->SizeOfRawData ? Section->Misc.VirtualSize : Section->SizeOfRawData;
			if (Rva >= SectionStart && Rva < SectionStart + SectionSize)
			{
				OutOffset = Section->PointerToRawData + (Rva - Section->VirtualAddress);
				return true;
			}
		}

		return false;
	}

	std::wstring FormatPdbStoreIndex(const GUID& Guid, DWORD Age, bool bHexAge)
	{
		WCHAR GuidText[64] = {};
		swprintf_s(
			GuidText,
			L"%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
			Guid.Data1,
			static_cast<unsigned int>(Guid.Data2),
			static_cast<unsigned int>(Guid.Data3),
			static_cast<unsigned int>(Guid.Data4[0]),
			static_cast<unsigned int>(Guid.Data4[1]),
			static_cast<unsigned int>(Guid.Data4[2]),
			static_cast<unsigned int>(Guid.Data4[3]),
			static_cast<unsigned int>(Guid.Data4[4]),
			static_cast<unsigned int>(Guid.Data4[5]),
			static_cast<unsigned int>(Guid.Data4[6]),
			static_cast<unsigned int>(Guid.Data4[7]));

		WCHAR AgeText[16] = {};
		if (bHexAge)
		{
			swprintf_s(AgeText, L"%lX", Age);
		}
		else
		{
			swprintf_s(AgeText, L"%lu", Age);
		}

		return std::wstring(GuidText) + AgeText;
	}

	bool TryReadPdbReferenceFromExecutable(const std::filesystem::path& ExecutablePath, FPdbReference& OutReference)
	{
		FScopedHandle File{ CreateFileW(
			ExecutablePath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr) };
		if (!File.IsValid())
		{
			return false;
		}

		LARGE_INTEGER FileSize = {};
		if (!GetFileSizeEx(File.Handle, &FileSize) || FileSize.QuadPart <= 0)
		{
			return false;
		}

		FScopedHandle Mapping{ CreateFileMappingW(File.Handle, nullptr, PAGE_READONLY, 0, 0, nullptr) };
		if (!Mapping.IsValid())
		{
			return false;
		}

		const BYTE* Base = static_cast<const BYTE*>(MapViewOfFile(Mapping.Handle, FILE_MAP_READ, 0, 0, 0));
		if (!Base)
		{
			return false;
		}

		const auto UnmapOnExit = std::unique_ptr<const BYTE, decltype(&UnmapViewOfFile)>(Base, UnmapViewOfFile);
		const uint64_t MappedSize = static_cast<uint64_t>(FileSize.QuadPart);
		if (!IsRangeInsideFile(0, sizeof(IMAGE_DOS_HEADER), MappedSize))
		{
			return false;
		}

		const IMAGE_DOS_HEADER* DosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
		if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE || DosHeader->e_lfanew < 0)
		{
			return false;
		}

		const uint64_t NtOffset = static_cast<uint64_t>(DosHeader->e_lfanew);
		if (!IsRangeInsideFile(NtOffset, sizeof(IMAGE_NT_HEADERS), MappedSize))
		{
			return false;
		}

		const IMAGE_NT_HEADERS* NtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(Base + NtOffset);
		if (NtHeaders->Signature != IMAGE_NT_SIGNATURE)
		{
			return false;
		}

		const IMAGE_DATA_DIRECTORY& DebugDirectory =
			NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
		if (DebugDirectory.VirtualAddress == 0 || DebugDirectory.Size < sizeof(IMAGE_DEBUG_DIRECTORY))
		{
			return false;
		}

		DWORD DebugDirectoryOffset = 0;
		if (!TryRvaToFileOffset(DebugDirectory.VirtualAddress, NtHeaders, DebugDirectoryOffset))
		{
			return false;
		}

		if (!IsRangeInsideFile(DebugDirectoryOffset, DebugDirectory.Size, MappedSize))
		{
			return false;
		}

		const size_t EntryCount = DebugDirectory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
		const IMAGE_DEBUG_DIRECTORY* Entries =
			reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(Base + DebugDirectoryOffset);

		for (size_t EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
		{
			const IMAGE_DEBUG_DIRECTORY& Entry = Entries[EntryIndex];
			if (Entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || Entry.SizeOfData <= offsetof(FCodeViewRsdsHeader, PdbPath))
			{
				continue;
			}

			DWORD CodeViewOffset = Entry.PointerToRawData;
			if (CodeViewOffset == 0 && Entry.AddressOfRawData != 0)
			{
				if (!TryRvaToFileOffset(Entry.AddressOfRawData, NtHeaders, CodeViewOffset))
				{
					continue;
				}
			}

			if (!IsRangeInsideFile(CodeViewOffset, Entry.SizeOfData, MappedSize))
			{
				continue;
			}

			const FCodeViewRsdsHeader* Rsds =
				reinterpret_cast<const FCodeViewRsdsHeader*>(Base + CodeViewOffset);
			if (Rsds->Signature != 0x53445352)
			{
				continue;
			}

			const size_t PdbPathCapacity = Entry.SizeOfData - offsetof(FCodeViewRsdsHeader, PdbPath);
			const char* PdbPathEnd = static_cast<const char*>(std::memchr(Rsds->PdbPath, '\0', PdbPathCapacity));
			const size_t PdbPathLength = PdbPathEnd ? static_cast<size_t>(PdbPathEnd - Rsds->PdbPath) : PdbPathCapacity;
			const std::string PdbPathText(Rsds->PdbPath, PdbPathLength);
			const std::wstring WidePdbPath = FPaths::ToWide(PdbPathText);

			OutReference.FileName = std::filesystem::path(WidePdbPath).filename().wstring();
			if (OutReference.FileName.empty())
			{
				OutReference.FileName = ExecutablePath.stem().wstring() + L".pdb";
			}

			OutReference.DecimalStoreIndex = FormatPdbStoreIndex(Rsds->Guid, Rsds->Age, false);
			OutReference.HexStoreIndex = FormatPdbStoreIndex(Rsds->Guid, Rsds->Age, true);
			return true;
		}

		return false;
	}

	bool TryCopyPdbFromSymbolStore(
		const std::filesystem::path& ExecutablePath,
		const std::wstring& SharedCrashDumpRoot,
		const std::filesystem::path& TargetDir)
	{
		std::filesystem::path CrashDumpRoot(SharedCrashDumpRoot);
		if (CrashDumpRoot.filename() != L"_crashdumps")
		{
			return false;
		}

		FPdbReference PdbReference;
		if (!TryReadPdbReferenceFromExecutable(ExecutablePath, PdbReference))
		{
			return false;
		}

		const std::filesystem::path SymbolServerRoot = CrashDumpRoot.parent_path();
		const std::filesystem::path TargetPdbPath = TargetDir / PdbReference.FileName;
		const std::filesystem::path DecimalCandidate =
			SymbolServerRoot / PdbReference.FileName / PdbReference.DecimalStoreIndex / PdbReference.FileName;
		if (CopyIfExists(DecimalCandidate, TargetPdbPath))
		{
			return true;
		}

		if (PdbReference.HexStoreIndex != PdbReference.DecimalStoreIndex)
		{
			const std::filesystem::path HexCandidate =
				SymbolServerRoot / PdbReference.FileName / PdbReference.HexStoreIndex / PdbReference.FileName;
			if (CopyIfExists(HexCandidate, TargetPdbPath))
			{
				return true;
			}
		}

		return false;
	}

	std::filesystem::path GetExecutablePath()
	{
		std::wstring Buffer(MAX_PATH, L'\0');
		for (;;)
		{
			const DWORD Length = GetModuleFileNameW(nullptr, Buffer.data(), static_cast<DWORD>(Buffer.size()));
			if (Length == 0)
			{
				return {};
			}

			if (Length < Buffer.size())
			{
				Buffer.resize(Length);
				return std::filesystem::path(Buffer);
			}

			Buffer.resize(Buffer.size() * 2);
		}
	}

	void WriteCrashCopyLog(
		const std::wstring& LocalDumpPath,
		const std::wstring& SharedDumpRoot,
		const std::wstring& SharedDumpPath,
		const std::wstring& FailureReason,
		const std::wstring& AuxiliaryCopyStatus,
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
			LogFile << "AuxiliaryCopyStatus: " << FPaths::ToUtf8(AuxiliaryCopyStatus) << "\n";
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
		std::wstring& OutFailureReason,
		std::wstring& OutAuxiliaryCopyStatus)
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
				const bool bExecutableCopied = CopyIfExists(ExecutablePath, SharedDir / ExecutablePath.filename());

				std::filesystem::path PdbPath = ExecutablePath;
				PdbPath.replace_extension(L".pdb");
				if (CopyIfExists(PdbPath, SharedDir / PdbPath.filename()))
				{
					OutAuxiliaryCopyStatus =
						std::wstring(bExecutableCopied ? L"Executable copied. " : L"Executable was not copied. ") +
						L"PDB copied from executable directory.";
				}
				else if (TryCopyPdbFromSymbolStore(ExecutablePath, SharedCrashDumpRoot, SharedDir))
				{
					OutAuxiliaryCopyStatus =
						std::wstring(bExecutableCopied ? L"Executable copied. " : L"Executable was not copied. ") +
						L"PDB copied from symbol store.";
				}
				else
				{
					OutAuxiliaryCopyStatus =
						std::wstring(bExecutableCopied ? L"Executable copied. " : L"Executable was not copied. ") +
						L"PDB was not copied. It was not next to the executable and a matching symbol-store entry was not found.";
				}
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

		IMAGEHLP_LINEW64 LineInfo = {};
		LineInfo.SizeOfStruct = sizeof(LineInfo);

		DWORD Displacement = 0;
		if (!SymGetLineFromAddrW64(Process, Address, &Displacement, &LineInfo))
		{
			return false;
		}

		wcscpy_s(OutLocation.File, LineInfo.FileName);
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
	volatile int* CrashAddress = nullptr;
	*CrashAddress = 0xDEAD;
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

		const MINIDUMP_TYPE DumpType = static_cast<MINIDUMP_TYPE>(
			MiniDumpWithDataSegs |
			MiniDumpWithThreadInfo |
			MiniDumpWithUnloadedModules |
			MiniDumpWithFullMemoryInfo |
			MiniDumpWithIndirectlyReferencedMemory |
			MiniDumpWithCodeSegs);

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			File,
			DumpType,
			&DumpInfo,
			nullptr,
			nullptr);

		CloseHandle(File);

		FSourceLocation ExceptionLocation;
		const bool bHasExceptionLocation = ResolveExceptionSource(ExceptionInfo, ExceptionLocation);
		std::wstring SharedDumpPath;
		std::wstring SharedDumpRoot;
		std::wstring SharedDumpFailureReason;
		std::wstring AuxiliaryCopyStatus;
		const bool bSharedDumpCopied = TryCopyDumpToSharedFolder(
			DumpPath,
			FileName,
			SharedDumpPath,
			SharedDumpRoot,
			SharedDumpFailureReason,
			AuxiliaryCopyStatus);
		WriteCrashCopyLog(
			DumpPath,
			SharedDumpRoot,
			SharedDumpPath,
			SharedDumpFailureReason,
			AuxiliaryCopyStatus,
			bSharedDumpCopied);

		WCHAR Message[4096];
		if (bHasExceptionLocation)
		{
			swprintf_s(Message, L"크래시 덤프가 저장되었습니다:\n%s\n\n공유 폴더 복사: %s\n%s\n\nException location:\n%ls:%lu",
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
