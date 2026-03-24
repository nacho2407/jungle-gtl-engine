#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Path.h"

#include <algorithm>
#include <filesystem>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Engine::SceneIO
{
    namespace Detail
    {
        inline FWString Utf8ToWide(const FString& InText)
        {
            if (InText.empty())
            {
                return {};
            }

#if defined(_WIN32)
            const int32 RequiredSize =
                MultiByteToWideChar(CP_UTF8, 0, InText.c_str(),
                                    static_cast<int32>(InText.size()), nullptr, 0);
            if (RequiredSize <= 0)
            {
                return {};
            }

            FWString OutText(static_cast<size_t>(RequiredSize), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, InText.c_str(), static_cast<int32>(InText.size()),
                                OutText.data(), RequiredSize);
            return OutText;
#else
            return FWString(InText.begin(), InText.end());
#endif
        }

        inline FString WideToUtf8(const FWString& InText)
        {
            if (InText.empty())
            {
                return {};
            }

#if defined(_WIN32)
            const int32 RequiredSize =
                WideCharToMultiByte(CP_UTF8, 0, InText.c_str(),
                                    static_cast<int32>(InText.size()), nullptr, 0, nullptr, nullptr);
            if (RequiredSize <= 0)
            {
                return {};
            }

            FString OutText(static_cast<size_t>(RequiredSize), '\0');
            WideCharToMultiByte(CP_UTF8, 0, InText.c_str(), static_cast<int32>(InText.size()),
                                OutText.data(), RequiredSize, nullptr, nullptr);
            return OutText;
#else
            return FString(InText.begin(), InText.end());
#endif
        }

        inline FString PathToUtf8String(const std::filesystem::path& Path)
        {
            const std::u8string Utf8Path = Path.generic_u8string();
            return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
        }

        inline bool IsUnderPath(const std::filesystem::path& Parent,
                                const std::filesystem::path& Child)
        {
            auto ParentIterator = Parent.begin();
            auto ChildIterator = Child.begin();

            for (; ParentIterator != Parent.end() && ChildIterator != Child.end();
                 ++ParentIterator, ++ChildIterator)
            {
                if (*ParentIterator != *ChildIterator)
                {
                    return false;
                }
            }

            return ParentIterator == Parent.end();
        }
    } // namespace Detail

    inline FString NormalizeSceneAssetPath(const FString& InPath)
    {
        if (InPath.empty())
        {
            return {};
        }

        FString Normalized = InPath;
        std::replace(Normalized.begin(), Normalized.end(), '\\', '/');

        if (Normalized.rfind("/Game", 0) == 0)
        {
            return Normalized;
        }

        std::filesystem::path CandidatePath = Detail::Utf8ToWide(Normalized);
        if (CandidatePath.empty())
        {
            return Normalized;
        }

        if (CandidatePath.is_relative())
        {
            CandidatePath = FPaths::Combine(FPaths::AppContentDir(), CandidatePath);
        }

        std::error_code ErrorCode;
        std::filesystem::path CanonicalContentRoot =
            std::filesystem::weakly_canonical(FPaths::AppContentDir(), ErrorCode);
        if (ErrorCode)
        {
            ErrorCode.clear();
            CanonicalContentRoot = FPaths::AppContentDir().lexically_normal();
        }

        std::filesystem::path CanonicalCandidate =
            std::filesystem::weakly_canonical(CandidatePath, ErrorCode);
        if (ErrorCode)
        {
            ErrorCode.clear();
            CanonicalCandidate = CandidatePath.lexically_normal();
        }

        if (!Detail::IsUnderPath(CanonicalContentRoot, CanonicalCandidate))
        {
            return Detail::PathToUtf8String(CanonicalCandidate);
        }

        std::filesystem::path RelativePath =
            CanonicalCandidate.lexically_relative(CanonicalContentRoot);
        FString GamePath = "/Game";
        for (const std::filesystem::path& Part : RelativePath)
        {
            if (Part.empty() || Part == ".")
            {
                continue;
            }

            GamePath += "/";
            GamePath += Detail::PathToUtf8String(Part);
        }

        return GamePath;
    }

    inline std::filesystem::path ResolveSceneAssetPathToAbsolute(const FString& InPath)
    {
        const FString Normalized = NormalizeSceneAssetPath(InPath);
        if (Normalized.empty())
        {
            return {};
        }

        if (Normalized.rfind("/Game", 0) != 0)
        {
            return std::filesystem::path(Detail::Utf8ToWide(Normalized));
        }

        FString RelativePath = Normalized.substr(5);
        if (!RelativePath.empty() && RelativePath.front() == '/')
        {
            RelativePath.erase(RelativePath.begin());
        }

        return FPaths::Combine(FPaths::AppContentDir(),
                               std::filesystem::path(Detail::Utf8ToWide(RelativePath)));
    }
} // namespace Engine::SceneIO
