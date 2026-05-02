#include "Editor/Packaging/GamePackager.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
    constexpr const wchar_t* MSBuildPath =
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\amd64\\MSBuild.exe";

    void EmitBuildLog(const FString& Message);
    void EmitBuildLogWidePath(const char* Prefix, const std::filesystem::path& Path);

    const char* GetConfigurationName(EGameBuildConfiguration Configuration)
    {
        return Configuration == EGameBuildConfiguration::Shipping
            ? "GameClientRelease"
            : "GameClientDebug";
    }

    std::filesystem::path FindSolutionRoot()
    {
        std::vector<std::filesystem::path> SearchRoots;
        SearchRoots.emplace_back(std::filesystem::path(FPaths::RootDir()));
        SearchRoots.emplace_back(std::filesystem::current_path());

        WCHAR ModulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, ModulePath, MAX_PATH) > 0)
        {
            SearchRoots.emplace_back(std::filesystem::path(ModulePath).parent_path());
        }

        for (std::filesystem::path Root : SearchRoots)
        {
            Root = Root.lexically_normal();
            while (!Root.empty())
            {
                if (std::filesystem::exists(Root / L"NipsEngine.sln"))
                {
                    return Root;
                }

                const std::filesystem::path Parent = Root.parent_path();
                if (Parent == Root)
                {
                    break;
                }
                Root = Parent;
            }
        }

        return {};
    }

    std::filesystem::path ResolveAgainstProjectRoot(const FString& Path)
    {
        std::filesystem::path Result(FPaths::ToWide(Path));
        if (Result.is_absolute())
        {
            return Result.lexically_normal();
        }
        const std::filesystem::path SolutionRoot = FindSolutionRoot();
        const std::filesystem::path Base = SolutionRoot.empty()
            ? std::filesystem::path(FPaths::RootDir())
            : SolutionRoot;
        return (Base / Result).lexically_normal();
    }

    std::filesystem::path ResolvePackageInputPath(const FString& Path)
    {
        std::filesystem::path Result(FPaths::ToWide(Path));
        if (Result.is_absolute())
        {
            return Result.lexically_normal();
        }

        const std::filesystem::path EngineRoot(FPaths::RootDir());
        const std::filesystem::path EngineRelative = (EngineRoot / Result).lexically_normal();
        if (std::filesystem::exists(EngineRelative))
        {
            return EngineRelative;
        }

        return ResolveAgainstProjectRoot(Path);
    }

    FString NormalizePackagePath(const std::filesystem::path& Path)
    {
        return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
    }

    FString EscapeRcPath(std::filesystem::path Path)
    {
        FString Result = FPaths::ToUtf8(Path.lexically_normal().wstring());
        for (size_t Index = 0; Index < Result.size(); ++Index)
        {
            if (Result[Index] == '\\')
            {
                Result.insert(Result.begin() + static_cast<std::ptrdiff_t>(Index), '\\');
                ++Index;
            }
            else if (Result[Index] == '"')
            {
                Result.insert(Result.begin() + static_cast<std::ptrdiff_t>(Index), '\\');
                ++Index;
            }
        }
        return Result;
    }

    FString ToLowerAscii(FString Value)
    {
        for (char& Ch : Value)
        {
            Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
        }
        return Value;
    }

    bool HasExtension(const FString& Path, std::initializer_list<const char*> Extensions)
    {
        const FString Ext = ToLowerAscii(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Path)).extension().generic_wstring()));
        for (const char* Allowed : Extensions)
        {
            if (Ext == Allowed)
            {
                return true;
            }
        }
        return false;
    }

    bool ValidateOptionalBrandingFile(const FString& Path, std::initializer_list<const char*> Extensions, const char* Label, FString& OutMessage)
    {
        if (Path.empty())
        {
            return true;
        }

        if (!HasExtension(Path, Extensions))
        {
            OutMessage = FString(Label) + " has an unsupported extension: " + Path;
            EmitBuildLog(OutMessage);
            return false;
        }

        const std::filesystem::path ResolvedPath = ResolvePackageInputPath(Path);
        if (!std::filesystem::exists(ResolvedPath))
        {
            OutMessage = FString(Label) + " not found: " + Path;
            EmitBuildLog(OutMessage);
            return false;
        }

        return true;
    }

    FString NormalizeSceneForPackage(const FString& Scene)
    {
        std::filesystem::path ScenePath(FPaths::ToWide(Scene));
        if (ScenePath.is_absolute())
        {
            return FPaths::ToRelativeString(ScenePath.wstring());
        }
        return FPaths::Normalize(Scene);
    }

    TArray<FString> BuildIncludedSceneList(const FGameBuildSettings& Settings)
    {
        TArray<FString> Scenes;
        const auto AddUnique = [&Scenes](const FString& Scene)
        {
            const FString Normalized = NormalizeSceneForPackage(Scene);
            if (Normalized.empty())
            {
                return;
            }
            for (const FString& Existing : Scenes)
            {
                if (FPaths::Normalize(Existing) == Normalized)
                {
                    return;
                }
            }
            Scenes.push_back(Normalized);
        };

        AddUnique(Settings.StartupScene);
        for (const FString& Scene : Settings.IncludedScenes)
        {
            AddUnique(Scene);
        }
        return Scenes;
    }

    bool CopyDirectoryIfExists(const std::filesystem::path& Source, const std::filesystem::path& Dest, FString& OutMessage)
    {
        std::error_code Ec;
        if (!std::filesystem::exists(Source, Ec))
        {
            return true;
        }

        std::filesystem::create_directories(Dest, Ec);
        if (Ec)
        {
            OutMessage = "Failed to create package directory: " + FPaths::ToUtf8(Dest.wstring());
            return false;
        }

        std::filesystem::copy(
            Source,
            Dest,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy package files: " + FPaths::ToUtf8(Source.wstring());
            return false;
        }
        return true;
    }

    std::filesystem::path ResolveStartupSceneForValidation(const FString& StartupScene)
    {
        std::filesystem::path ScenePath(FPaths::ToWide(StartupScene));
        if (ScenePath.is_absolute())
        {
            return ScenePath.lexically_normal();
        }
        return std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(StartupScene))).lexically_normal();
    }

    void EmitBuildLog(const FString& Message)
    {
        UE_LOG("[Build] %s", Message.c_str());
    }

    void EmitBuildLogWidePath(const char* Prefix, const std::filesystem::path& Path)
    {
        UE_LOG("[Build] %s%s", Prefix, FPaths::ToUtf8(Path.wstring()).c_str());
    }

    void FlushProcessOutput(FString& Buffer, bool bFlushPartialLine)
    {
        size_t Start = 0;
        while (Start < Buffer.size())
        {
            const size_t End = Buffer.find_first_of("\r\n", Start);
            if (End == FString::npos)
            {
                break;
            }

            FString Line = Buffer.substr(Start, End - Start);
            if (!Line.empty())
            {
                EmitBuildLog(Line);
            }

            Start = End + 1;
            while (Start < Buffer.size() && (Buffer[Start] == '\r' || Buffer[Start] == '\n'))
            {
                ++Start;
            }
        }

        Buffer.erase(0, Start);
        if (bFlushPartialLine && !Buffer.empty())
        {
            EmitBuildLog(Buffer);
            Buffer.clear();
        }
    }

    bool ReadAvailablePipeOutput(HANDLE ReadPipe, FString& PendingOutput)
    {
        DWORD AvailableBytes = 0;
        if (!PeekNamedPipe(ReadPipe, nullptr, 0, nullptr, &AvailableBytes, nullptr))
        {
            return false;
        }

        if (AvailableBytes == 0)
        {
            return true;
        }

        std::vector<char> Buffer(std::min<DWORD>(AvailableBytes, 4096));
        DWORD BytesRead = 0;
        if (!ReadFile(ReadPipe, Buffer.data(), static_cast<DWORD>(Buffer.size()), &BytesRead, nullptr) || BytesRead == 0)
        {
            return false;
        }

        PendingOutput.append(Buffer.data(), BytesRead);
        FlushProcessOutput(PendingOutput, false);
        return true;
    }

    bool ValidateStartupSceneForPackaging(const FGameBuildSettings& Settings, FString& OutMessage)
    {
        const std::filesystem::path ScenePath = ResolveStartupSceneForValidation(Settings.StartupScene);
        EmitBuildLogWidePath("Validating startup scene = ", ScenePath);

        std::ifstream SceneFile(ScenePath);
        if (!SceneFile.is_open())
        {
            OutMessage = "Startup scene not found: " + FPaths::ToUtf8(ScenePath.wstring());
            EmitBuildLog(OutMessage);
            return false;
        }

        FString SceneJson((std::istreambuf_iterator<char>(SceneFile)), std::istreambuf_iterator<char>());
        json::JSON Root = json::JSON::Load(SceneJson);
        if (!Root.hasKey("Actors"))
        {
            OutMessage = "Startup scene is invalid: Actors section missing";
            EmitBuildLog(OutMessage);
            return false;
        }

        int32 PlayerStartCount = 0;
        json::JSON& Actors = Root["Actors"];
        for (int32 Index = 0; Index < static_cast<int32>(Actors.length()); ++Index)
        {
            json::JSON& Data = Actors.at(Index);
            if (Data.hasKey("ClassName") && Data["ClassName"].ToString() == "APlayerStart")
            {
                ++PlayerStartCount;
            }
        }

        if (PlayerStartCount != 1)
        {
            OutMessage = "Packaging validation failed: Startup scene must contain exactly one Player Start.";
            UE_LOG("[Build] %s Found=%d", OutMessage.c_str(), PlayerStartCount);
            EmitBuildLog("Add a Player Start to the startup scene, save it, then build again.");
            return false;
        }

        EmitBuildLog("Startup scene validation passed");
        return true;
    }

    bool ValidateIncludedScenesForPackaging(const FGameBuildSettings& Settings, FString& OutMessage)
    {
        const TArray<FString> Scenes = BuildIncludedSceneList(Settings);
        UE_LOG("[Build] Validating scene copy list. Count=%d", static_cast<int32>(Scenes.size()));
        for (const FString& Scene : Scenes)
        {
            const std::filesystem::path ScenePath = ResolveStartupSceneForValidation(Scene);
            if (!std::filesystem::exists(ScenePath))
            {
                OutMessage = "Scene to copy not found: " + Scene;
                EmitBuildLog(OutMessage);
                return false;
            }
        }
        return true;
    }
}

FGamePackageResult FGamePackager::BuildAndPackage(const FGameBuildSettings& Settings)
{
    FGamePackageResult Result;
    Result.OutputDirectory = FPaths::ToUtf8(ResolveAgainstProjectRoot(Settings.OutputDirectory).wstring());

    FString Message;
    if (!ValidateStartupSceneForPackaging(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!ValidateIncludedScenesForPackaging(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!PrepareBrandingResources(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!RunMSBuild(Settings, Message))
    {
        FString IgnoredResetMessage;
        PrepareBrandingResources(FGameBuildSettings{}, IgnoredResetMessage);
        Result.Message = Message;
        UE_LOG("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    FString IgnoredResetMessage;
    PrepareBrandingResources(FGameBuildSettings{}, IgnoredResetMessage);

    if (!CopyPackageFiles(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    Result.bSucceeded = true;
    Result.Message = "Game package created";
    UE_LOG("[Build] Build Complete");
    return Result;
}

bool FGamePackager::PrepareBrandingResources(const FGameBuildSettings& Settings, FString& OutMessage)
{
    if (!ValidateOptionalBrandingFile(Settings.IconPath, { ".ico" }, "Game icon", OutMessage))
    {
        return false;
    }
    if (!ValidateOptionalBrandingFile(Settings.SplashImagePath, { ".png", ".jpg", ".jpeg", ".bmp" }, "Splash image", OutMessage))
    {
        return false;
    }

    const std::filesystem::path EngineRoot(FPaths::RootDir());
    const std::filesystem::path GeneratedDir = EngineRoot / L"Source" / L"Engine" / L"Runtime";
    const std::filesystem::path RcPath = GeneratedDir / L"GameBranding.rc";
    std::error_code Ec;
    std::filesystem::create_directories(GeneratedDir, Ec);
    if (Ec)
    {
        OutMessage = "Failed to create branding resource directory";
        EmitBuildLog(OutMessage);
        return false;
    }

    std::ofstream RcFile(RcPath, std::ios::trunc);
    if (!RcFile.is_open())
    {
        OutMessage = "Failed to write GameBranding.rc";
        EmitBuildLog(OutMessage);
        return false;
    }

    RcFile << "// Auto-generated by Packaging. Do not edit by hand.\n";
    if (!Settings.IconPath.empty())
    {
        const std::filesystem::path IconPath = ResolvePackageInputPath(Settings.IconPath);
        RcFile << "#define IDI_GAME_ICON 101\n";
        RcFile << "IDI_GAME_ICON ICON \"" << EscapeRcPath(IconPath) << "\"\n";
        EmitBuildLogWidePath("Generated exe icon resource = ", IconPath);
    }
    else
    {
        EmitBuildLog("Generated empty branding resource. No game icon selected.");
    }
    return true;
}

bool FGamePackager::RunMSBuild(const FGameBuildSettings& Settings, FString& OutMessage)
{
    const std::filesystem::path SolutionRoot = FindSolutionRoot();
    const std::filesystem::path SolutionPath = SolutionRoot / L"NipsEngine.sln";
    if (!std::filesystem::exists(SolutionPath))
    {
        OutMessage = "NipsEngine.sln not found";
        EmitBuildLog("NipsEngine.sln not found");
        EmitBuildLogWidePath("FPaths::RootDir = ", std::filesystem::path(FPaths::RootDir()));
        EmitBuildLogWidePath("Current path    = ", std::filesystem::current_path());
        return false;
    }

    const FString Configuration = GetConfigurationName(Settings.Configuration);
    EmitBuildLog("Starting MSBuild");
    EmitBuildLogWidePath("Solution = ", SolutionPath);
    UE_LOG("[Build] Configuration = %s", Configuration.c_str());

    std::wstring CommandLine =
        L"\"" + std::wstring(MSBuildPath) + L"\" \"" + SolutionPath.wstring()
        + L"\" /p:Configuration=" + FPaths::ToWide(Configuration)
        + L" /p:Platform=x64 /v:minimal";

    SECURITY_ATTRIBUTES SecurityAttributes = {};
    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.bInheritHandle = TRUE;

    HANDLE ReadPipe = nullptr;
    HANDLE WritePipe = nullptr;
    if (!CreatePipe(&ReadPipe, &WritePipe, &SecurityAttributes, 0))
    {
        OutMessage = "Failed to create MSBuild log pipe";
        EmitBuildLog(OutMessage);
        return false;
    }
    SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    StartupInfo.wShowWindow = SW_HIDE;
    StartupInfo.hStdOutput = WritePipe;
    StartupInfo.hStdError = WritePipe;
    StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION ProcessInfo = {};
    std::wstring MutableCommandLine = CommandLine;
    if (!CreateProcessW(nullptr, MutableCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        SolutionRoot.wstring().c_str(), &StartupInfo, &ProcessInfo))
    {
        CloseHandle(ReadPipe);
        CloseHandle(WritePipe);
        OutMessage = "Failed to start MSBuild";
        EmitBuildLog(OutMessage);
        return false;
    }

    CloseHandle(WritePipe);
    FString PendingOutput;
    while (WaitForSingleObject(ProcessInfo.hProcess, 50) == WAIT_TIMEOUT)
    {
        if (!ReadAvailablePipeOutput(ReadPipe, PendingOutput))
        {
            break;
        }
    }

    while (ReadAvailablePipeOutput(ReadPipe, PendingOutput))
    {
        DWORD AvailableBytes = 0;
        if (!PeekNamedPipe(ReadPipe, nullptr, 0, nullptr, &AvailableBytes, nullptr) || AvailableBytes == 0)
        {
            break;
        }
    }
    FlushProcessOutput(PendingOutput, true);
    CloseHandle(ReadPipe);

    DWORD ExitCode = 1;
    GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    if (ExitCode != 0)
    {
        OutMessage = "MSBuild failed";
        UE_LOG("[Build] MSBuild failed. ExitCode=%lu", ExitCode);
        return false;
    }

    EmitBuildLog("MSBuild succeeded");
    return true;
}

bool FGamePackager::CopyPackageFiles(const FGameBuildSettings& Settings, FString& OutMessage)
{
    const std::filesystem::path EngineRoot(FPaths::RootDir());
    const std::filesystem::path OutputRoot = ResolveAgainstProjectRoot(Settings.OutputDirectory);
    const FString Configuration = GetConfigurationName(Settings.Configuration);
    const std::filesystem::path BuiltExe = EngineRoot / L"Bin" / FPaths::ToWide(Configuration) / L"NipsGame.exe";
    const std::filesystem::path BuiltPdb = EngineRoot / L"Bin" / FPaths::ToWide(Configuration) / L"NipsGame.pdb";

    std::error_code Ec;
    if (!std::filesystem::exists(BuiltExe, Ec))
    {
        OutMessage = "Built NipsGame.exe not found";
        EmitBuildLog(OutMessage);
        EmitBuildLogWidePath("Expected exe = ", BuiltExe);
        return false;
    }

    if (Settings.bCleanOutput)
    {
        EmitBuildLogWidePath("Cleaning output = ", OutputRoot);
        std::filesystem::remove_all(OutputRoot, Ec);
        if (Ec)
        {
            OutMessage = "Failed to clean output directory";
            EmitBuildLog(OutMessage);
            return false;
        }
    }

    EmitBuildLogWidePath("Packaging to = ", OutputRoot);
    std::filesystem::create_directories(OutputRoot, Ec);
    if (Ec)
    {
        OutMessage = "Failed to create output directory";
        EmitBuildLog(OutMessage);
        return false;
    }

    std::filesystem::copy_file(BuiltExe, OutputRoot / L"NipsGame.exe",
                               std::filesystem::copy_options::overwrite_existing, Ec);
    if (Ec)
    {
        OutMessage = "Failed to copy NipsGame.exe";
        EmitBuildLog(OutMessage);
        return false;
    }
    EmitBuildLog("Copied NipsGame.exe");

    if (std::filesystem::exists(BuiltPdb, Ec))
    {
        std::filesystem::copy_file(BuiltPdb, OutputRoot / L"NipsGame.pdb",
                                   std::filesystem::copy_options::overwrite_existing, Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy NipsGame.pdb";
            EmitBuildLog(OutMessage);
            return false;
        }
        EmitBuildLog("Copied NipsGame.pdb");
    }

    if (!CopyDirectoryIfExists(EngineRoot / L"Asset", OutputRoot / L"Asset", OutMessage)) return false;
    EmitBuildLog("Copied Asset directory");
    const TArray<FString> IncludedScenes = BuildIncludedSceneList(Settings);
    for (const FString& Scene : IncludedScenes)
    {
        const std::filesystem::path SourceScene = ResolveStartupSceneForValidation(Scene);
        const std::filesystem::path RelativeScene(FPaths::ToWide(NormalizeSceneForPackage(Scene)));
        const std::filesystem::path DestScene = (OutputRoot / RelativeScene).lexically_normal();
        std::filesystem::create_directories(DestScene.parent_path(), Ec);
        if (Ec)
        {
            OutMessage = "Failed to create scene package directory";
            EmitBuildLog(OutMessage);
            return false;
        }
        std::filesystem::copy_file(SourceScene, DestScene, std::filesystem::copy_options::overwrite_existing, Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy scene: " + Scene;
            EmitBuildLog(OutMessage);
            return false;
        }
        UE_LOG("[Build] Copied scene = %s", Scene.c_str());
    }
    if (!CopyDirectoryIfExists(EngineRoot / L"Shaders", OutputRoot / L"Shaders", OutMessage)) return false;
    EmitBuildLog("Copied Shaders directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"LuaScript", OutputRoot / L"LuaScript", OutMessage)) return false;
    EmitBuildLog("Copied LuaScript directory");

    FString PackagedIconPath;
    FString PackagedSplashPath;
    if (!Settings.IconPath.empty() || !Settings.SplashImagePath.empty())
    {
        const std::filesystem::path BrandingDir = OutputRoot / L"Branding";
        std::filesystem::create_directories(BrandingDir, Ec);
        if (Ec)
        {
            OutMessage = "Failed to create Branding directory";
            EmitBuildLog(OutMessage);
            return false;
        }

        if (!Settings.IconPath.empty())
        {
            const std::filesystem::path SourceIcon = ResolvePackageInputPath(Settings.IconPath);
            const std::filesystem::path DestIcon = BrandingDir / SourceIcon.filename();
            std::filesystem::copy_file(SourceIcon, DestIcon, std::filesystem::copy_options::overwrite_existing, Ec);
            if (Ec)
            {
                OutMessage = "Failed to copy game icon";
                EmitBuildLog(OutMessage);
                return false;
            }
            PackagedIconPath = NormalizePackagePath(std::filesystem::path(L"Branding") / SourceIcon.filename());
            EmitBuildLog("Copied game icon");
        }

        if (!Settings.SplashImagePath.empty())
        {
            const std::filesystem::path SourceSplash = ResolvePackageInputPath(Settings.SplashImagePath);
            const std::filesystem::path DestSplash = BrandingDir / SourceSplash.filename();
            std::filesystem::copy_file(SourceSplash, DestSplash, std::filesystem::copy_options::overwrite_existing, Ec);
            if (Ec)
            {
                OutMessage = "Failed to copy splash image";
                EmitBuildLog(OutMessage);
                return false;
            }
            PackagedSplashPath = NormalizePackagePath(std::filesystem::path(L"Branding") / SourceSplash.filename());
            EmitBuildLog("Copied splash image");
        }
    }

    std::filesystem::create_directories(OutputRoot / L"Settings", Ec);
    std::filesystem::create_directories(OutputRoot / L"Saves", Ec);

    std::ofstream GameIni(OutputRoot / L"Settings" / L"Game.ini", std::ios::trunc);
    if (!GameIni.is_open())
    {
        OutMessage = "Failed to write Game.ini";
        EmitBuildLog(OutMessage);
        return false;
    }

    GameIni << "[Game]\n";
    GameIni << "GameName=" << Settings.GameName << "\n";
    GameIni << "StartupScene=" << NormalizeSceneForPackage(Settings.StartupScene) << "\n";
    GameIni << "PlayerControllerClass=" << Settings.PlayerControllerClass << "\n";
    GameIni << "\n[Scenes]\n";
    GameIni << "Count=" << IncludedScenes.size() << "\n";
    for (size_t SceneIndex = 0; SceneIndex < IncludedScenes.size(); ++SceneIndex)
    {
        GameIni << "Scene" << SceneIndex << "=" << IncludedScenes[SceneIndex] << "\n";
    }
    GameIni << "\n[Branding]\n";
    GameIni << "Icon=" << PackagedIconPath << "\n";
    GameIni << "Splash=" << PackagedSplashPath << "\n";
    GameIni << "SplashMinSeconds=" << std::max(3.0f, Settings.SplashMinSeconds) << "\n";
    GameIni.close();
    EmitBuildLog("Wrote Settings/Game.ini");

    if (Settings.bRunAfterBuild)
    {
        const std::filesystem::path PackageExe = OutputRoot / L"NipsGame.exe";
        STARTUPINFOW StartupInfo = {};
        StartupInfo.cb = sizeof(StartupInfo);
        PROCESS_INFORMATION ProcessInfo = {};
        std::wstring CommandLine = L"\"" + PackageExe.wstring() + L"\"";
        if (!CreateProcessW(nullptr, CommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            OutputRoot.wstring().c_str(), &StartupInfo, &ProcessInfo))
        {
            OutMessage = "Package created, but failed to run NipsGame.exe";
            return false;
        }
        CloseHandle(ProcessInfo.hThread);
        CloseHandle(ProcessInfo.hProcess);
    }

    return true;
}
