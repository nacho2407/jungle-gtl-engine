#pragma once

#include "Core/CoreMinimal.h"

#include <filesystem>

struct FEditorSettingsData
{
    float GridSpacing = 20.0f;
    float CameraMoveSpeed = 100.0f;
    float CameraRotationSpeed = 0.3f;
    float ContentBrowserLeftPaneWidth = 250.0f;
};

enum class EEditorSettingsLoadResult
{
    Success,
    Missing,
    InvalidFormat,
    IOError
};

class FEditorSettings
{
  public:
    EEditorSettingsLoadResult Load(FEditorSettingsData& OutData,
                                   FString* OutErrorMessage = nullptr) const;
    bool Save(const FEditorSettingsData& InData) const;

  private:
    std::filesystem::path GetSettingsFilePath() const;
};
