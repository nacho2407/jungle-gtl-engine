#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

struct FEditorSettings
{
	// Viewport
	float CameraSpeed = 10.f;
	float CameraRotationSpeed = 60.f;
	float CameraZoomSpeed = 300.f;
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	// Runtime
	bool bLimitUpdateRate = true;
	int32 UpdateRate = 60;

	// File paths
	FString DefaultSavePath = "Asset/Scene/";

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static constexpr const char* DefaultSettingsPath = "Settings/Editor.ini";
};
