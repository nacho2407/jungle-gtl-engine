#pragma once

#include "Core/CoreTypes.h"
#include "Core/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"

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

	// Viewport Layout
	int32 LayoutType = 0; // EViewportLayout
	FViewportRenderOptions SlotOptions[4];
	float SplitterRatios[3] = { 0.5f, 0.5f, 0.5f };
	int32 SplitterCount = 0;

	// Perspective Camera (slot 0) 복원용
	FVector PerspCamLocation = FVector(10, 0, 5);
	FVector PerspCamRotation = FVector(0, 0, 0);
	float PerspCamFOV = 60.0f;
	float PerspCamNearClip = 0.1f;
	float PerspCamFarClip = 1000.0f;

	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
