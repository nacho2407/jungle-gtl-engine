#include "Editor/UI/EditorMainPanelPackagingHelpers.h"

#include "Engine/Core/Paths.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <commdlg.h>
#include <cwctype>
#include <filesystem>

const char* FEditorMainPanelPackagingHelpers::GetPopupName()
{
    return "Packaging Settings";
}

FString FEditorMainPanelPackagingHelpers::SanitizePackageNameForPath(const FString& Name)
{
    FString Result = Name;
    for (char& Ch : Result)
    {
        const bool bInvalid =
            Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' ||
            Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*';
        if (bInvalid || static_cast<unsigned char>(Ch) < 32)
        {
            Ch = '_';
        }
    }

    const size_t Start = Result.find_first_not_of(" .\t\r\n");
    if (Start == FString::npos)
    {
        return "JSEngineGame";
    }

    const size_t End = Result.find_last_not_of(" .\t\r\n");
    Result = Result.substr(Start, End - Start + 1);
    return Result.empty() ? FString("JSEngineGame") : Result;
}

FString FEditorMainPanelPackagingHelpers::MakeDefaultPackageOutputDirectory(const FString& GameName)
{
    return FPaths::Normalize("Builds/Windows/" + SanitizePackageNameForPath(GameName));
}

bool FEditorMainPanelPackagingHelpers::IsDefaultPackageOutputDirectory(
    const FString& OutputDirectory,
    const FString& GameName
)
{
    return FPaths::Normalize(OutputDirectory) == MakeDefaultPackageOutputDirectory(GameName);
}

FString FEditorMainPanelPackagingHelpers::NormalizePackagingScenePath(const FString& ScenePath)
{
    if (ScenePath.empty())
    {
        return {};
    }

    std::filesystem::path Path(FPaths::ToWide(ScenePath));
    if (Path.is_absolute())
    {
        return FPaths::Normalize(FPaths::ToRelativeString(Path.wstring()));
    }
    return FPaths::Normalize(ScenePath);
}

bool FEditorMainPanelPackagingHelpers::AddUniquePackagingScene(TArray<FString>& Scenes, const FString& ScenePath)
{
    const FString NormalizedScene = NormalizePackagingScenePath(ScenePath);
    if (NormalizedScene.empty())
    {
        return false;
    }

    for (const FString& Existing : Scenes)
    {
        if (FPaths::Normalize(Existing) == NormalizedScene)
        {
            return false;
        }
    }

    Scenes.push_back(NormalizedScene);
    return true;
}

bool FEditorMainPanelPackagingHelpers::OpenPackagingAssetFileDialog(const wchar_t* Filter, FString& OutFilePath)
{
    OutFilePath.clear();

    WCHAR FileBuffer[MAX_PATH] = {};
    OPENFILENAMEW DialogDesc = {};
    DialogDesc.lStructSize = sizeof(DialogDesc);
    DialogDesc.hwndOwner = ImGui::GetMainViewport()
        ? static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw)
        : nullptr;
    DialogDesc.lpstrFilter = Filter;
    DialogDesc.lpstrFile = FileBuffer;
    DialogDesc.nMaxFile = MAX_PATH;
    DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    const std::filesystem::path PrevCwd = std::filesystem::current_path();
    const BOOL bPicked = GetOpenFileNameW(&DialogDesc);
    std::error_code RestoreEc;
    std::filesystem::current_path(PrevCwd, RestoreEc);
    if (!bPicked)
    {
        return false;
    }

    OutFilePath = FPaths::ToRelativeString(FileBuffer);
    if (OutFilePath.empty())
    {
        OutFilePath = FPaths::ToUtf8(FileBuffer);
    }
    return true;
}

std::wstring FEditorMainPanelPackagingHelpers::ToLowerPathExtension(const FString& Path)
{
    std::wstring Ext = std::filesystem::path(FPaths::ToWide(Path)).extension().wstring();
    std::transform(Ext.begin(), Ext.end(), Ext.begin(), [](wchar_t Ch)
    {
        return static_cast<wchar_t>(std::towlower(Ch));
    });
    return Ext;
}
