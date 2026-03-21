#pragma once

#include "Core/CoreTypes.h"
#include "Core/Paths.h"
#include "Math/Vector.h"

enum class EViewMode : int32
{
	Lit = 0,
	Unlit,
	Wireframe,
	Count
};

struct FShowFlags
{
	bool bPrimitives = true;
	bool bGrid = true;
	bool bGizmo = true;
};

struct FEditorSettings
{
	// Viewport
	float CameraSpeed = 10.f;
	float CameraRotationSpeed = 60.f;
	float CameraZoomSpeed = 300.f;
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	// View
	EViewMode ViewMode = EViewMode::Lit;
	FShowFlags ShowFlags;

	// Runtime
	bool bLimitUpdateRate = true;
	int32 UpdateRate = 60;

	// File paths
	FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
