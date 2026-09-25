#include "EditorSettings.h"

#include "Core/Path.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    using FIniSection = TMap<FString, FString>;
    using FIniDocument = TMap<FString, FIniSection>;
    using ELoadResult = EEditorSettingsLoadResult;

    FString TrimCopy(const FString& InValue)
    {
        const size_t Start = InValue.find_first_not_of(" \t\r\n");
        if (Start == FString::npos)
        {
            return {};
        }

        const size_t End = InValue.find_last_not_of(" \t\r\n");
        return InValue.substr(Start, End - Start + 1);
    }

    bool TryParseFloat(const FString& InValue, float& OutValue)
    {
        const FString Trimmed = TrimCopy(InValue);
        if (Trimmed.empty())
        {
            return false;
        }

        char* EndPtr = nullptr;
        const float ParsedValue = std::strtof(Trimmed.c_str(), &EndPtr);
        if (EndPtr == Trimmed.c_str() || *EndPtr != '\0')
        {
            return false;
        }

        OutValue = ParsedValue;
        return true;
    }

    ELoadResult LoadIniDocument(const std::filesystem::path& FilePath, FIniDocument& OutDocument,
                                FString* OutErrorMessage)
    {
        std::error_code ErrorCode;
        if (!std::filesystem::exists(FilePath, ErrorCode))
        {
            return ELoadResult::Missing;
        }

        std::ifstream File(FilePath);
        if (!File.is_open())
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "Failed to open settings file.";
            }

            return ELoadResult::IOError;
        }

        FString CurrentSection;
        FString Line;
        int32 LineNumber = 0;
        while (std::getline(File, Line))
        {
            ++LineNumber;
            const FString TrimmedLine = TrimCopy(Line);
            if (TrimmedLine.empty() || TrimmedLine[0] == ';' || TrimmedLine[0] == '#')
            {
                continue;
            }

            if (TrimmedLine.front() == '[')
            {
                if (TrimmedLine.back() != ']')
                {
                    if (OutErrorMessage != nullptr)
                    {
                        *OutErrorMessage =
                            "Line " + std::to_string(LineNumber) + " has an invalid section header.";
                    }

                    return ELoadResult::InvalidFormat;
                }

                CurrentSection = TrimCopy(TrimmedLine.substr(1, TrimmedLine.size() - 2));
                if (CurrentSection.empty())
                {
                    if (OutErrorMessage != nullptr)
                    {
                        *OutErrorMessage =
                            "Line " + std::to_string(LineNumber) + " has an empty section name.";
                    }

                    return ELoadResult::InvalidFormat;
                }

                OutDocument[CurrentSection];
                continue;
            }

            const size_t EqualsIndex = TrimmedLine.find('=');
            if (EqualsIndex == FString::npos)
            {
                if (OutErrorMessage != nullptr)
                {
                    *OutErrorMessage = "Line " + std::to_string(LineNumber)
                        + " is not a valid key=value entry.";
                }

                return ELoadResult::InvalidFormat;
            }

            if (CurrentSection.empty())
            {
                if (OutErrorMessage != nullptr)
                {
                    *OutErrorMessage = "Line " + std::to_string(LineNumber)
                        + " is outside of any section.";
                }

                return ELoadResult::InvalidFormat;
            }

            const FString Key = TrimCopy(TrimmedLine.substr(0, EqualsIndex));
            const FString Value = TrimCopy(TrimmedLine.substr(EqualsIndex + 1));
            if (Key.empty() || Value.empty())
            {
                if (OutErrorMessage != nullptr)
                {
                    *OutErrorMessage = "Line " + std::to_string(LineNumber)
                        + " has an empty key or value.";
                }

                return ELoadResult::InvalidFormat;
            }

            OutDocument[CurrentSection][Key] = Value;
        }

        return ELoadResult::Success;
    }

    bool SaveIniDocument(const std::filesystem::path& FilePath, const FIniDocument& Document)
    {
        std::error_code ErrorCode;
        std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);

        std::ofstream File(FilePath, std::ios::trunc);
        if (!File.is_open())
        {
            return false;
        }

        TArray<FString> SectionNames;
        SectionNames.reserve(Document.size());
        for (const auto& [SectionName, _] : Document)
        {
            SectionNames.push_back(SectionName);
        }

        std::sort(SectionNames.begin(), SectionNames.end());

        bool bFirstSection = true;
        for (const FString& SectionName : SectionNames)
        {
            const auto SectionIt = Document.find(SectionName);
            if (SectionIt == Document.end())
            {
                continue;
            }

            if (!bFirstSection)
            {
                File << '\n';
            }
            bFirstSection = false;

            if (!SectionName.empty())
            {
                File << '[' << SectionName << "]\n";
            }

            TArray<FString> Keys;
            Keys.reserve(SectionIt->second.size());
            for (const auto& [Key, _] : SectionIt->second)
            {
                Keys.push_back(Key);
            }

            std::sort(Keys.begin(), Keys.end());

            for (const FString& Key : Keys)
            {
                const auto ValueIt = SectionIt->second.find(Key);
                if (ValueIt == SectionIt->second.end())
                {
                    continue;
                }

                File << Key << '=' << ValueIt->second << '\n';
            }
        }

        return true;
    }

    const FString* FindIniValue(const FIniDocument& Document, const FString& SectionName,
                                const FString& Key)
    {
        const auto SectionIt = Document.find(SectionName);
        if (SectionIt == Document.end())
        {
            return nullptr;
        }

        const auto ValueIt = SectionIt->second.find(Key);
        if (ValueIt == SectionIt->second.end())
        {
            return nullptr;
        }

        return &ValueIt->second;
    }

    void SetIniValue(FIniDocument& Document, const FString& SectionName, const FString& Key,
                     const FString& Value)
    {
        Document[SectionName][Key] = Value;
    }

    FString FormatFloat(float Value)
    {
        std::ostringstream Stream;
        Stream << std::fixed << std::setprecision(3) << Value;
        return Stream.str();
    }
} // namespace

EEditorSettingsLoadResult FEditorSettings::Load(FEditorSettingsData& OutData,
                                                FString* OutErrorMessage) const
{
    FIniDocument Document;
    const ELoadResult LoadResult = LoadIniDocument(GetSettingsFilePath(), Document, OutErrorMessage);
    if (LoadResult != ELoadResult::Success)
    {
        return LoadResult;
    }

    if (const FString* GridSpacingValue = FindIniValue(Document, "Viewport", "GridSpacing"))
    {
        float ParsedGridSpacing = OutData.GridSpacing;
        if (!TryParseFloat(*GridSpacingValue, ParsedGridSpacing))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "[Viewport] GridSpacing must be a float value.";
            }

            return ELoadResult::InvalidFormat;
        }

        OutData.GridSpacing = ParsedGridSpacing;
    }

    if (const FString* CameraMoveSpeedValue =
            FindIniValue(Document, "Viewport", "CameraMoveSpeed"))
    {
        float ParsedMoveSpeed = OutData.CameraMoveSpeed;
        if (!TryParseFloat(*CameraMoveSpeedValue, ParsedMoveSpeed))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "[Viewport] CameraMoveSpeed must be a float value.";
            }

            return ELoadResult::InvalidFormat;
        }

        OutData.CameraMoveSpeed = ParsedMoveSpeed;
    }

    if (const FString* CameraRotationSpeedValue =
            FindIniValue(Document, "Viewport", "CameraRotationSpeed"))
    {
        float ParsedRotationSpeed = OutData.CameraRotationSpeed;
        if (!TryParseFloat(*CameraRotationSpeedValue, ParsedRotationSpeed))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "[Viewport] CameraRotationSpeed must be a float value.";
            }

            return ELoadResult::InvalidFormat;
        }

        OutData.CameraRotationSpeed = ParsedRotationSpeed;
    }

    if (const FString* LeftPaneWidthValue =
            FindIniValue(Document, "ContentBrowser", "LeftPaneWidth"))
    {
        float ParsedLeftPaneWidth = OutData.ContentBrowserLeftPaneWidth;
        if (!TryParseFloat(*LeftPaneWidthValue, ParsedLeftPaneWidth))
        {
            if (OutErrorMessage != nullptr)
            {
                *OutErrorMessage = "[ContentBrowser] LeftPaneWidth must be a float value.";
            }

            return ELoadResult::InvalidFormat;
        }

        OutData.ContentBrowserLeftPaneWidth = ParsedLeftPaneWidth;
    }

    return ELoadResult::Success;
}

bool FEditorSettings::Save(const FEditorSettingsData& InData) const
{
    const std::filesystem::path FilePath = GetSettingsFilePath();

    FIniDocument Document;
    LoadIniDocument(FilePath, Document, nullptr);

    SetIniValue(Document, "Viewport", "GridSpacing", FormatFloat(InData.GridSpacing));
    SetIniValue(Document, "Viewport", "CameraMoveSpeed",
                FormatFloat(InData.CameraMoveSpeed));
    SetIniValue(Document, "Viewport", "CameraRotationSpeed",
                FormatFloat(InData.CameraRotationSpeed));
    SetIniValue(Document, "ContentBrowser", "LeftPaneWidth",
                FormatFloat(InData.ContentBrowserLeftPaneWidth));
    return SaveIniDocument(FilePath, Document);
}

std::filesystem::path FEditorSettings::GetSettingsFilePath() const
{
    return FPaths::Combine(FPaths::Combine(FPaths::SavedDir(), L"Config"), L"editor.ini");
}
