#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Platform/Paths.h"
#include "Physics/IPhysicsScene.h"  // EPhysicsBackend

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
		uint32 SpotAtlasResolution = 4096;	// Spot Light Atlas page 해상도
		uint32 PointAtlasResolution = 4096;	// Point Light Atlas page 해상도
		uint32 MaxSpotAtlasPages   = 4;		// Spot Light Atlas 최대 page 수
		uint32 MaxPointAtlasPages  = 4;		// Point Light Atlas 최대 page 수
	};

	// --- Physics ---
	struct FPhysicsOption
	{
		EPhysicsBackend Backend = EPhysicsBackend::Native;
	};

	// --- Game ---
	struct FGameOption
	{
		FString StartLevelName;     // Scene 파일 이름 (확장자 제외)
		FString GameModeClassName;  // ""면 GameEngine이 코드로 지정한 디폴트 사용.
		                            // 잘못된 이름이거나 AGameModeBase 파생이 아니면 디폴트 fallback.
	};

public:
	FShadowOption Shadow;
	FPhysicsOption Physics;
	FGameOption Game;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
