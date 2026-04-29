#pragma once

#include "Core/CoreTypes.h"
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
		uint32 SpotAtlasResolution = 4096;	// Spot Light Atlas page 해상도
		uint32 PointAtlasResolution = 4096;	// Point Light Atlas page 해상도
		uint32 MaxSpotAtlasPages   = 4;		// Spot Light Atlas 최대 page 수
		uint32 MaxPointAtlasPages  = 4;		// Point Light Atlas 최대 page 수
	};

public:
	FShadowOption Shadow;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
