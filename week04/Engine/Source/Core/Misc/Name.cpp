#include "Core/CoreMinimal.h"
#include "Name.h"
#include "NameSubsystem.h"
#include <algorithm> // std::transform
#include <cctype>    // std::tolower

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unicode/ustring.h>
#endif

namespace Engine::Core::Misc
{
    FName::FName(const char* InStr) : ComparisonIndex(IndexNone), DisplayIndex(IndexNone)
    {
        if (InStr && InStr[0] != '\0')
        {
            FString Str(InStr);
            ComparisonIndex = FNameSubsystem::Get().FindOrAdd(std::move(ToLower(Str)));
            DisplayIndex = FNameSubsystem::Get().FindOrAdd(Str);
        }
    }

    FName::FName(const wchar_t* InWStr) : ComparisonIndex(IndexNone), DisplayIndex(IndexNone)
    {
        if (InWStr && InWStr[0] == L'\0')
        {
            FString Str{ConvertToFString(InWStr)};
            if (!Str.empty())
            {
                ComparisonIndex = FNameSubsystem::Get().FindOrAdd(std::move(ToLower(Str)));
                DisplayIndex = FNameSubsystem::Get().FindOrAdd(Str);
            }
        }
    }

    FName::FName(const FString& InStr) : ComparisonIndex(IndexNone), DisplayIndex(IndexNone)
    {
        ComparisonIndex = FNameSubsystem::Get().FindOrAdd(std::move(ToLower(InStr)));
        DisplayIndex = FNameSubsystem::Get().FindOrAdd(InStr);
    }

    FName::FName(const FWString& InStr) : ComparisonIndex(IndexNone), DisplayIndex(IndexNone)
    {
        FString Str{ConvertToFString(InStr)};
        if (!Str.empty())
        {
            ComparisonIndex = FNameSubsystem::Get().FindOrAdd(std::move(ToLower(Str)));
            DisplayIndex = FNameSubsystem::Get().FindOrAdd(Str);
        }
    }

    FString FName::ToFString() const { return FNameSubsystem::Get().GetString(DisplayIndex); }

    FWString FName::ToFWString() const
    {
        return ConvertToFWString(FNameSubsystem::Get().GetString(DisplayIndex));
    }

    FWString FName::ConvertToFWString(const FString& InStr)
    {
        if (InStr.empty())
        {
            return FWString{};
        }
#if defined(_WIN32)
        int32 SizeNeeded{
            MultiByteToWideChar(CP_UTF8, 0, &InStr[0], static_cast<int32>(InStr.size()), NULL, 0)};
        FWString OutWStr(SizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &InStr[0], static_cast<int32>(InStr.size()), &OutWStr[0],
                            SizeNeeded);
        return OutWStr;
#else
        UErrorCode Status{U_ZERO_ERROR};
        int32_t    TargetCapacity{0};

        u_strFromUTF8(nullptr, 0, &TargetCapacity, InStr.c_str(),
                      static_cast<int32_t>(InStr.size()), &Status);

        if (U_FAILURE(Status) && Status != U_BUFFER_OVERFLOW_ERROR)
            return L"";

        Status = U_ZERO_ERROR;
        FWString OutWStr(TargetCapacity, 0);

        u_strFromUTF8(reinterpret_cast<UChar*>(&OutWStr[0]), TargetCapacity, nullptr, InStr.c_str(),
                      static_cast<int32_t>(InStr.size()), &Status);

        return OutWStr;
#endif
    }

    FString FName::ConvertToFString(const wchar_t* InWStr)
    {
        if (!InWStr || InWStr[0] == L'\0')
            return "";
#if defined(_WIN32)
        int32   WLen{static_cast<int32>(wcslen(InWStr))};
        int32   SizeNeeded{WideCharToMultiByte(CP_UTF8, 0, InWStr, WLen, NULL, 0, NULL, NULL)};
        FString OutStr(SizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, InWStr, WLen, &OutStr[0], SizeNeeded, NULL, NULL);
        return OutStr;
#else
        UErrorCode Status{U_ZERO_ERROR};
        int32_t    TargetCapacity{0};
        int32_t    WLen = static_cast<int32_t>(wcslen(InWStr));

        u_strToUTF8(nullptr, 0, &TargetCapacity, reinterpret_cast<const UChar*>(InWStr), WLen,
                    &Status);

        if (U_FAILURE(Status) && Status != U_BUFFER_OVERFLOW_ERROR)
            return "";

        Status = U_ZERO_ERROR;
        FString OutStr(TargetCapacity, 0);

        u_strToUTF8(&OutStr[0], TargetCapacity, nullptr, reinterpret_cast<const UChar*>(InWStr),
                    WLen, &Status);

        if (U_FAILURE(Status))
            return "";

        return OutStr;
#endif
    }

    FString FName::ConvertToFString(const FWString& InWStr)
    {
        if (InWStr.empty())
        {
            return FString{};
        }
#if defined(_WIN32)
        int32   SizeNeeded{WideCharToMultiByte(
            CP_UTF8, 0, &InWStr[0], static_cast<int32>(InWStr.size()), NULL, 0, NULL, NULL)};
        FString OutStr(SizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, &InWStr[0], static_cast<int32>(InWStr.size()), &OutStr[0],
                            SizeNeeded, NULL, NULL);
        return OutStr;
#else
        UErrorCode Status{U_ZERO_ERROR};
        int32_t    TargetCapacity{0};

        u_strToUTF8(nullptr, 0, &TargetCapacity, reinterpret_cast<const UChar*>(InWStr.c_str()),
                    static_cast<int32_t>(InWStr.size()), &Status);

        if (U_FAILURE(Status) && Status != U_BUFFER_OVERFLOW_ERROR)
            return "";

        Status = U_ZERO_ERROR;
        FString OutStr(TargetCapacity, 0);

        u_strToUTF8(&OutStr[0], TargetCapacity, nullptr,
                    reinterpret_cast<const UChar*>(InWStr.c_str()),
                    static_cast<int32_t>(InWStr.size()), &Status);

        if (U_FAILURE(Status))
            return "";

        return OutStr;
#endif
    }

    FString FName::ToLower(FString InStr)
    {
        FString OutStr{InStr};
        for (auto& C : OutStr)
        {
            if (static_cast<uint8>(C) < 0x7F)
                if ('A' <= C && C <= 'Z')
                {
                    C += 32;
                }
        }
        return OutStr;
    }
} // namespace Engine::Core::Misc
