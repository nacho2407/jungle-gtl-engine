#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Common/ViewTypes.h"

enum class EModelUpAxis
{
	Z_up = 0, // Blender, Unreal
	Y_up = 1  // Maya, Unity
};

class FObjViewerSettings : public TSingleton<FObjViewerSettings>
{
	friend class TSingleton<FObjViewerSettings>;

protected:
	// FObjViewer만의 기본값 세팅
	FObjViewerSettings()
	{
		ViewMode = EViewMode::Lit;
		ShowFlags.bAxis = false;
		ShowFlags.bBillboardText = false;
	}

public:
	// Viewport
	float CameraSpeed = 10.f;
	float CameraRotationSpeed = 60.f;
	float CameraForwardSpeed = 500.f;
	FVector InitViewPos = FVector(0, 0, 0);
	FVector InitLookAt = FVector(0, 0, 0);

	// View
	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;

	// Grid
	float GridSpacing = 1.0f;
	int32 GridHalfLineCount = 100;

	// Camera Sensitivity
	float CameraMoveSensitivity = 0.5f;
	float CameraRotateSensitivity = 0.1f;
	
	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());
	EModelUpAxis ModelUpAxis = EModelUpAxis::Z_up;

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::ViewerSettingsFilePath()); }
};
