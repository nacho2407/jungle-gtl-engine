#include "Editor/Build/EditorGameBuilder.h"

#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <windows.h>
#include <Shellapi.h>

namespace
{
struct FProcessResult
{
	DWORD ExitCode = 0;
	FString Output;
};

std::wstring Quote(const std::wstring& Text)
{
	return L"\"" + Text + L"\"";
}

std::wstring GetEnvironmentVariableString(const wchar_t* Name)
{
	DWORD Size = GetEnvironmentVariableW(Name, nullptr, 0);
	if (Size == 0)
	{
		return {};
	}

	std::wstring Result(Size, L'\0');
	GetEnvironmentVariableW(Name, Result.data(), Size);
	while (!Result.empty() && Result.back() == L'\0')
	{
		Result.pop_back();
	}
	return Result;
}

FString TrimProcessOutputLine(FString Text)
{
	while (!Text.empty() && (Text.back() == '\r' || Text.back() == '\n' || Text.back() == ' ' || Text.back() == '\t'))
	{
		Text.pop_back();
	}

	const FString::size_type FirstBreak = Text.find_first_of("\r\n");
	if (FirstBreak != FString::npos)
	{
		Text.resize(FirstBreak);
	}
	return Text;
}

FString WidePathToUtf8(const std::filesystem::path& Path)
{
	return FPaths::ToUtf8(Path.wstring());
}

std::filesystem::path ResolveDirectoryPath(const FString& PathText, const std::filesystem::path& BaseDirectory)
{
	std::filesystem::path Path(FPaths::ToWide(PathText));
	if (Path.is_absolute())
	{
		return Path.lexically_normal();
	}
	return (BaseDirectory / Path).lexically_normal();
}

std::filesystem::path GetDefaultSolutionRoot()
{
	// 기존 구현과 같은 방식의 fallback solution root
	const std::filesystem::path EngineRoot(FPaths::RootDir());
	return EngineRoot.lexically_normal().parent_path().lexically_normal();
}

std::filesystem::path ResolveSolutionPath(const FEditorGameBuildSettings& InSettings, FString& InOutLog)
{
	const std::filesystem::path DefaultSolutionRoot = GetDefaultSolutionRoot();
	if (InSettings.SolutionPath.empty())
	{
		const std::filesystem::path FallbackPath = (DefaultSolutionRoot / L"KraftonEngine.sln").lexically_normal();
		InOutLog += "[Build] Solution path is empty. Using default: " + WidePathToUtf8(FallbackPath) + "\n";
		return FallbackPath;
	}

	// UI에서 선택한 절대 경로를 우선 사용하되, 수동 입력된 상대 경로는 기존 solution root 기준으로 보정
	std::filesystem::path SolutionPath(FPaths::ToWide(InSettings.SolutionPath));
	if (!SolutionPath.is_absolute())
	{
		SolutionPath = DefaultSolutionRoot / SolutionPath;
	}

	return SolutionPath.lexically_normal();
}

bool IsUnsafeCleanTarget(const std::filesystem::path& Target, const std::filesystem::path& SolutionRoot, const std::filesystem::path& EngineRoot)
{
	if (Target.empty())
	{
		return true;
	}

	const std::filesystem::path NormalTarget = Target.lexically_normal();
	return NormalTarget == SolutionRoot.lexically_normal()
		|| NormalTarget == EngineRoot.lexically_normal()
		|| NormalTarget == NormalTarget.root_path();
}

bool RunProcessCapture(
	const std::filesystem::path& ApplicationPath,
	const std::wstring& Arguments,
	const std::filesystem::path& WorkingDirectory,
	FProcessResult& OutResult)
{
	SECURITY_ATTRIBUTES SecurityAttributes = {};
	SecurityAttributes.nLength = sizeof(SecurityAttributes);
	SecurityAttributes.bInheritHandle = TRUE;

	HANDLE ReadPipe = nullptr;
	HANDLE WritePipe = nullptr;
	if (!CreatePipe(&ReadPipe, &WritePipe, &SecurityAttributes, 0))
	{
		OutResult.Output += "[Build] CreatePipe failed.\n";
		return false;
	}
	SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW StartupInfo = {};
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.hStdOutput = WritePipe;
	StartupInfo.hStdError = WritePipe;
	StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION ProcessInfo = {};
	std::wstring CommandLine = Quote(ApplicationPath.wstring());
	if (!Arguments.empty())
	{
		CommandLine += L" ";
		CommandLine += Arguments;
	}

	const BOOL bCreated = CreateProcessW(
		ApplicationPath.c_str(),
		CommandLine.data(),
		nullptr,
		nullptr,
		TRUE,
		CREATE_NO_WINDOW,
		nullptr,
		WorkingDirectory.empty() ? nullptr : WorkingDirectory.c_str(),
		&StartupInfo,
		&ProcessInfo);

	CloseHandle(WritePipe);
	WritePipe = nullptr;

	if (!bCreated)
	{
		OutResult.Output += "[Build] CreateProcessW failed. Error=" + std::to_string(GetLastError()) + "\n";
		CloseHandle(ReadPipe);
		return false;
	}

	char Buffer[4096];
	DWORD BytesRead = 0;
	while (ReadFile(ReadPipe, Buffer, static_cast<DWORD>(sizeof(Buffer) - 1), &BytesRead, nullptr) && BytesRead > 0)
	{
		Buffer[BytesRead] = '\0';
		OutResult.Output.append(Buffer, BytesRead);
	}

	WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
	GetExitCodeProcess(ProcessInfo.hProcess, &OutResult.ExitCode);

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	CloseHandle(ReadPipe);
	return true;
}

std::filesystem::path FindMSBuildPath(FString& InOutLog)
{
	const std::wstring ProgramFilesX86 = GetEnvironmentVariableString(L"ProgramFiles(x86)");
	if (!ProgramFilesX86.empty())
	{
		const std::filesystem::path VsWherePath =
			std::filesystem::path(ProgramFilesX86) / L"Microsoft Visual Studio" / L"Installer" / L"vswhere.exe";
		if (std::filesystem::exists(VsWherePath))
		{
			FProcessResult Result;
			const std::wstring Args = L"-latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";
			if (RunProcessCapture(VsWherePath, Args, VsWherePath.parent_path(), Result))
			{
				FString Candidate = TrimProcessOutputLine(Result.Output);
				if (!Candidate.empty())
				{
					std::filesystem::path CandidatePath(FPaths::ToWide(Candidate));
					if (std::filesystem::exists(CandidatePath))
					{
						InOutLog += "[Build] MSBuild found by vswhere: " + Candidate + "\n";
						return CandidatePath;
					}
				}
			}
		}
	}

	const std::filesystem::path Fallbacks[] = {
		L"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe",
		L"C:/Program Files/Microsoft Visual Studio/17/Community/MSBuild/Current/Bin/MSBuild.exe",
		L"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
		L"C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
	};

	for (const std::filesystem::path& Path : Fallbacks)
	{
		if (std::filesystem::exists(Path))
		{
			InOutLog += "[Build] MSBuild found by fallback: " + WidePathToUtf8(Path) + "\n";
			return Path;
		}
	}

	return {};
}

bool CopySingleFile(const std::filesystem::path& Source, const std::filesystem::path& Destination, FString& InOutLog)
{
	std::error_code Error;
	std::filesystem::create_directories(Destination.parent_path(), Error);
	Error.clear();
	std::filesystem::copy_file(Source, Destination, std::filesystem::copy_options::overwrite_existing, Error);
	if (Error)
	{
		InOutLog += "[Build] Copy failed: " + WidePathToUtf8(Source) + " -> " + WidePathToUtf8(Destination) + "\n";
		InOutLog += "[Build] " + Error.message() + "\n";
		return false;
	}
	return true;
}

bool CopyDirectoryRecursive(const std::filesystem::path& Source, const std::filesystem::path& Destination, FString& InOutLog)
{
	if (!std::filesystem::exists(Source))
	{
		InOutLog += "[Build] Directory missing: " + WidePathToUtf8(Source) + "\n";
		return false;
	}

	std::error_code Error;
	std::filesystem::create_directories(Destination, Error);
	Error.clear();
	std::filesystem::copy(
		Source,
		Destination,
		std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
		Error);
	if (Error)
	{
		InOutLog += "[Build] Directory copy failed: " + WidePathToUtf8(Source) + " -> " + WidePathToUtf8(Destination) + "\n";
		InOutLog += "[Build] " + Error.message() + "\n";
		return false;
	}
	return true;
}
}

FEditorGameBuilder::~FEditorGameBuilder()
{
	Shutdown();
}

bool FEditorGameBuilder::StartBuild(const FEditorGameBuildSettings& InSettings)
{
	if (bBuilding.load())
	{
		return false;
	}

	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}

	ResetStateForBuild();
	bBuilding.store(true);
	WorkerThread = std::thread(&FEditorGameBuilder::BuildThreadProc, this, InSettings);
	return true;
}

void FEditorGameBuilder::Shutdown()
{
	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}
}

bool FEditorGameBuilder::HasFinished() const
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	return bFinished;
}

bool FEditorGameBuilder::WasSuccessful() const
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	return bSucceeded;
}

FString FEditorGameBuilder::GetStatusText() const
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	return StatusText;
}

FString FEditorGameBuilder::GetLogText() const
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	return LogText;
}

FString FEditorGameBuilder::GetLastOutputDirectory() const
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	return LastOutputDirectory;
}

void FEditorGameBuilder::OpenOutputFolder() const
{
	const FString OutputDirectory = GetLastOutputDirectory();
	if (OutputDirectory.empty())
	{
		return;
	}
	ShellExecuteW(nullptr, L"open", FPaths::ToWide(OutputDirectory).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void FEditorGameBuilder::RunGame() const
{
	const FString OutputDirectory = GetLastOutputDirectory();
	if (OutputDirectory.empty())
	{
		return;
	}

	std::filesystem::path OutputPath(FPaths::ToWide(OutputDirectory));
	std::filesystem::path ExePath = OutputPath / L"KraftonEngine.exe";
	if (!std::filesystem::exists(ExePath))
	{
		return;
	}

	STARTUPINFOW StartupInfo = {};
	StartupInfo.cb = sizeof(StartupInfo);
	PROCESS_INFORMATION ProcessInfo = {};
	std::wstring CommandLine = Quote(ExePath.wstring());
	if (CreateProcessW(
		ExePath.c_str(),
		CommandLine.data(),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		OutputPath.c_str(),
		&StartupInfo,
		&ProcessInfo))
	{
		CloseHandle(ProcessInfo.hThread);
		CloseHandle(ProcessInfo.hProcess);
	}
}

void FEditorGameBuilder::BuildThreadProc(FEditorGameBuildSettings InSettings)
{
	bool bSuccess = false;

	FString LocalLog;
	const std::filesystem::path SolutionPath = ResolveSolutionPath(InSettings, LocalLog);
	const std::filesystem::path SolutionRoot = SolutionPath.parent_path().lexically_normal();
	const std::filesystem::path EngineRoot = (SolutionRoot / L"KraftonEngine").lexically_normal();
	const std::filesystem::path OutputDirectory = ResolveDirectoryPath(InSettings.OutputDirectory, SolutionRoot);
	const std::filesystem::path BuildOutputDirectory =
		EngineRoot / L"Bin" / FPaths::ToWide(InSettings.Configuration);

	{
		std::lock_guard<std::mutex> Lock(StateMutex);
		LastOutputDirectory = WidePathToUtf8(OutputDirectory);
	}

	LocalLog += "[Build] Solution: " + WidePathToUtf8(SolutionPath) + "\n";
	LocalLog += "[Build] EngineRoot: " + WidePathToUtf8(EngineRoot) + "\n";
	LocalLog += "[Build] Output: " + WidePathToUtf8(OutputDirectory) + "\n";
	AppendLog(LocalLog);
	LocalLog.clear();

	if (SolutionPath.extension() != L".sln")
	{
		AppendLog("[Build] Selected solution path is not a .sln file.\n");
		SetStatus("Failed: invalid solution file");
		SetFinished(false);
		bBuilding.store(false);
		return;
	}

	if (!std::filesystem::exists(SolutionPath))
	{
		AppendLog("[Build] Solution file not found.\n");
		SetStatus("Failed: solution not found");
		SetFinished(false);
		bBuilding.store(false);
		return;
	}

	if (!std::filesystem::exists(EngineRoot) || !std::filesystem::exists(EngineRoot / L"KraftonEngine.vcxproj"))
	{
		AppendLog("[Build] Engine root not found next to selected solution.\n");
		AppendLog("[Build] Expected: " + WidePathToUtf8(EngineRoot) + "\n");
		SetStatus("Failed: engine root not found");
		SetFinished(false);
		bBuilding.store(false);
		return;
	}

	if (InSettings.bBuildExecutable)
	{
		SetStatus("Finding MSBuild");
		FString FindLog;
		const std::filesystem::path MSBuildPath = FindMSBuildPath(FindLog);
		AppendLog(FindLog);
		if (MSBuildPath.empty())
		{
			AppendLog("[Build] MSBuild.exe not found.\n");
			SetStatus("Failed: MSBuild not found");
			SetFinished(false);
			bBuilding.store(false);
			return;
		}

		SetStatus("Running MSBuild");
		const std::wstring Args =
			Quote(SolutionPath.wstring()) +
			L" /p:Configuration=" + FPaths::ToWide(InSettings.Configuration) +
			L" /p:Platform=" + FPaths::ToWide(InSettings.Platform) +
			L" /m /v:minimal";

		FProcessResult BuildResult;
		if (!RunProcessCapture(MSBuildPath, Args, SolutionRoot, BuildResult))
		{
			AppendLog(BuildResult.Output);
			SetStatus("Failed: MSBuild process error");
			SetFinished(false);
			bBuilding.store(false);
			return;
		}

		AppendLog(BuildResult.Output);
		if (BuildResult.ExitCode != 0)
		{
			AppendLog("[Build] MSBuild failed. ExitCode=" + std::to_string(BuildResult.ExitCode) + "\n");
			SetStatus("Failed: MSBuild error");
			SetFinished(false);
			bBuilding.store(false);
			return;
		}
	}
	else
	{
		AppendLog("[Build] MSBuild skipped by settings.\n");
	}

	SetStatus("Packaging");

	std::error_code Error;
	if (InSettings.bCleanOutput && std::filesystem::exists(OutputDirectory))
	{
		if (IsUnsafeCleanTarget(OutputDirectory, SolutionRoot, EngineRoot))
		{
			AppendLog("[Build] Refusing to clean unsafe output directory.\n");
			SetStatus("Failed: unsafe output directory");
			SetFinished(false);
			bBuilding.store(false);
			return;
		}

		std::filesystem::remove_all(OutputDirectory, Error);
		if (Error)
		{
			AppendLog("[Build] Failed to clean output directory. Close the running packaged game and try again.\n");
			AppendLog("[Build] " + Error.message() + "\n");
			SetStatus("Failed: output directory locked");
			SetFinished(false);
			bBuilding.store(false);
			return;
		}
	}

	Error.clear();
	std::filesystem::create_directories(OutputDirectory, Error);
	if (Error)
	{
		AppendLog("[Build] Failed to create output directory: " + Error.message() + "\n");
		SetStatus("Failed: output directory");
		SetFinished(false);
		bBuilding.store(false);
		return;
	}

	bSuccess = true;
	const std::filesystem::path ExeSource = BuildOutputDirectory / L"KraftonEngine.exe";
	bSuccess &= CopySingleFile(ExeSource, OutputDirectory / L"KraftonEngine.exe", LocalLog);

	if (InSettings.bPackageDlls && std::filesystem::exists(BuildOutputDirectory))
	{
		for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(BuildOutputDirectory))
		{
			if (Entry.is_regular_file() && Entry.path().extension() == L".dll")
			{
				bSuccess &= CopySingleFile(Entry.path(), OutputDirectory / Entry.path().filename(), LocalLog);
			}
		}
	}

	if (InSettings.bPackageContent)
	{
		bSuccess &= CopyDirectoryRecursive(EngineRoot / L"Content", OutputDirectory / L"Content", LocalLog);
	}
	if (InSettings.bPackageShaders)
	{
		bSuccess &= CopyDirectoryRecursive(EngineRoot / L"Shaders", OutputDirectory / L"Shaders", LocalLog);
	}
	if (InSettings.bPackageSettings)
	{
		bSuccess &= CopyDirectoryRecursive(EngineRoot / L"Settings", OutputDirectory / L"Settings", LocalLog);
	}

	AppendLog(LocalLog);
	SetStatus(bSuccess ? "Build complete" : "Failed: packaging error");
	SetFinished(bSuccess);
	bBuilding.store(false);
}

void FEditorGameBuilder::ResetStateForBuild()
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	LogText.clear();
	StatusText = "Starting";
	LastOutputDirectory.clear();
	bFinished = false;
	bSucceeded = false;
}

void FEditorGameBuilder::AppendLog(const FString& Text)
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	LogText += Text;
}

void FEditorGameBuilder::SetStatus(const FString& Text)
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	StatusText = Text;
}

void FEditorGameBuilder::SetFinished(bool bInSucceeded)
{
	std::lock_guard<std::mutex> Lock(StateMutex);
	bFinished = true;
	bSucceeded = bInSucceeded;
}
