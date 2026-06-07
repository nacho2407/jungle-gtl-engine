#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Platform/Paths.h"

/*
	FProjectSettings — 프로젝트 전역 설정 (per-viewport가 아닌 전체 공유).
	Settings/ProjectSettings.ini에 독립 직렬화됩니다.
*/

class FProjectSettings : public TSingleton<FProjectSettings>
{
	friend class TSingleton<FProjectSettings>;

	// --- Shadow ---
	struct FShadowOption
	{
		bool bEnabled = true;
		uint32 CSMResolution       = 2048;	// Directional Light CSM cascade 해상도
		float DirectionalShadowDistance = 300.0f;	// CSM 적용 거리. 너무 크게 잡으면 cascade가 카메라 회전에 심하게 흔들립니다.
		bool bDirectionalShadowFadeOut = false;	// 마지막 cascade 거리 fade-out 사용 여부
		uint32 SpotAtlasResolution = 4096;	// Spot Light Atlas page 해상도
		uint32 PointAtlasResolution = 4096;	// Point Light Atlas page 해상도
		uint32 MaxSpotAtlasPages   = 4;		// Spot Light Atlas 최대 page 수
		uint32 MaxPointAtlasPages  = 4;		// Point Light Atlas 최대 page 수
	};

	// --- Game ---
	struct FGameplayPreset
	{
		FString DirectorModule = "Game.FractureDirector";
		FString PlayerControllerClassName = "ALuaPlayerController";
		FString DefaultPawnClassName = "ALuaCharacter";
		FString DefaultPawnScript = "Player/KainCharacter.lua";
		FString DefaultPawnMeshPath = "Content/Data/Samba Dancing (10).fbx";
		FString DefaultPlayerStartTag = "Default";
		bool bUsePlacedAutoPossessPawn = true;
		bool bSpawnDefaultPawnIfMissing = true;
	};

	struct FGameOption
	{
		FString StartLevelName;     // Scene 파일 이름 (확장자 제외)
		FGameplayPreset GameplayPreset;
	};

	struct FPhysicsOption
	{
		bool bEnablePvd = false;
		bool bPvdTransmitContacts = false;
		bool bPvdTransmitSceneQueries = false;
		bool bPvdTransmitConstraints = false;

		float FixedTimeStep = 1.0f / 60.0f;
		int32 MaxSubSteps = 4;
		float MaxAccumulatedTime = 0.25f;
	};

	struct FDiagnosticsOption
	{
		FString CrashDumpShareDir;
	};

public:
	FShadowOption Shadow;
	FGameOption Game;
	FPhysicsOption Physics;
	FDiagnosticsOption Diagnostics;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
