#pragma once

#include "Engine/Core/Containers/Array.h"
#include "Engine/Core/Containers/String.h"

#include <string>

class FEditorMainPanelPackagingHelpers
{
public:
    static const char* GetPopupName();

    static FString SanitizePackageNameForPath(const FString& Name);
    static FString MakeDefaultPackageOutputDirectory(const FString& GameName);
    static bool IsDefaultPackageOutputDirectory(const FString& OutputDirectory, const FString& GameName);

    static FString NormalizePackagingScenePath(const FString& ScenePath);
    static bool AddUniquePackagingScene(TArray<FString>& Scenes, const FString& ScenePath);

    static bool OpenPackagingAssetFileDialog(const wchar_t* Filter, FString& OutFilePath);
    static std::wstring ToLowerPathExtension(const FString& Path);
};
