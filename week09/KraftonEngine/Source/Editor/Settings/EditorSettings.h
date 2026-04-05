#pragma once

#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
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
	FRotator PerspCamRotation;
	float PerspCamFOV = 60.0f;
	float PerspCamNearClip = 0.1f;
	float PerspCamFarClip = 1000.0f;

	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

	// UI 위젯 표시 여부
	struct FUIVisibility
	{
#ifdef FPS_OPTIMIZATION
		bool bConsole = false;
		bool bControl = false;
		bool bProperty = false;
		bool bScene = true;
		bool bStat = true;
#else
		bool bConsole = true;
		bool bControl = true;
		bool bProperty = true;
		bool bScene = true;
		bool bStat = false;;
#endif
	} UI;

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
