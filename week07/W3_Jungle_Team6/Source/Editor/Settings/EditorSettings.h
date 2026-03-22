#pragma once

#include "Core/CoreTypes.h"
#include "Core/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Common/ViewTypes.h"

class FEditorSettings : public TSingleton<FEditorSettings>
{
	friend class TSingleton<FEditorSettings>;

public:
	// Viewport
	float CameraSpeed = 10.f;
	float CameraRotationSpeed = 60.f;
	float CameraZoomSpeed = 300.f;
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	// View
	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;

	// Grid
	float GridSpacing = 1.0f;
	int32 GridHalfLineCount = 100;

	// Camera Sensitivity
	float CameraMoveSensitivity = 1.0f;
	float CameraRotateSensitivity = 1.0f;

	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
