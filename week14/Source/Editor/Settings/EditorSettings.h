#pragma once

#include "EditorViewportSettings.h"
#include "GizmoToolSettings.h"
#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Platform/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Render/Types/ViewTypes.h"

class FEditorSettings : public TSingleton<FEditorSettings>
{
	friend class TSingleton<FEditorSettings>;

public:
	/**
	* @brief 기본 뷰포트 배경색을 반환합니다.
	*
	* @return 기본 뷰포트 배경색
	*/
	static FLinearColor GetDefaultViewportBackgroundColor()
	{
		return FLinearColor(0.12f, 0.12f, 0.13f, 1.0f);
	}

	// Viewport
	FVector InitViewPos = FVector(10, 0, 5);
	FVector InitLookAt = FVector(0, 0, 0);

	// Viewport Layout
	int32 LayoutType = 0; // EViewportLayout
	float SplitterRatios[3] = { 0.5f, 0.5f, 0.5f };
	int32 SplitterCount = 0;

	// Perspective Camera (slot 0) 복원용
	FVector PerspCamLocation = FVector(10, 0, 5);
	FRotator PerspCamRotation;
	float PerspCamFOV = 60.0f;
	float PerspCamNearClip = 0.1f;
	float PerspCamFarClip = 10000.0f;

	FViewportCameraControlSettings LevelViewportCameraControls;
	FEditorViewportSettings LevelViewportSettings[4];
	FEditorViewportSettings MeshEditorViewportSettings;
	/**
	* @brief 모든 에디터 뷰포트 공용 초기화 배경색
	*/
	FLinearColor ViewportBackgroundColor = GetDefaultViewportBackgroundColor();

	// File paths
	FString EditorStartLevel;  // 비어있으면 빈 씬, 씬 파일명(확장자 제외)이면 자동 로드
	FString ContentBrowserPath; // 비어있으면 프로젝트 루트

	// UI 위젯 표시 여부
	struct FUIVisibility
	{
		bool bConsole = true;
		bool bControl = true;
		bool bProperty = true;
		bool bScene = true;
		bool bStat = false;
		bool bContentBrowser = true;
		bool bImGUISettings = false;
		bool bEditorDebug = false;
		bool bShadowMapDebug = false;
		bool bAnimationDebug = false;
		bool bWallRunDebug = true;
	} UI;

	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
