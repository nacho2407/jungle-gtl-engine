#pragma once

#include "Core/Containers/String.h"
#include "Core/Paths.h"

// Editor 전용 이미지/아이콘은 게임 Asset과 섞지 않고 Resources 아래에서 관리합니다.
namespace FEditorResourcePaths
{
    inline constexpr const char* Root = "Resources/Editor/";
    inline constexpr const char* Icons = "Resources/Editor/Icons/";
    inline constexpr const char* ToolIcons = "Resources/Editor/ToolIcons/";
    inline constexpr const char* Branding = "Resources/Editor/Branding/";

    inline FString Icon(const char* FileName)
    {
        return FString(Icons) + FileName;
    }

    inline FString ToolIcon(const char* FileName)
    {
        return FString(ToolIcons) + FileName;
    }

    inline FString BrandingFile(const char* FileName)
    {
        return FString(Branding) + FileName;
    }

    inline std::wstring IconsAbsoluteDir()
    {
        return FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Icons));
    }

    inline std::wstring ToolIconsAbsoluteDir()
    {
        return FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(ToolIcons));
    }

    inline std::wstring BrandingAbsoluteFile(const char* FileName)
    {
        return FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(BrandingFile(FileName)));
    }
}
