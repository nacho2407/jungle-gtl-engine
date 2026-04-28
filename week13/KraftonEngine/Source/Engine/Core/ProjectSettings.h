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
		bool bPSM = false;		// Perspective Shadow Maps (per-viewport, 카메라 연동)
		uint32 MaxSpotAtlasPages  = 4;	// Spot Light Atlas 최대 page 수
		uint32 MaxPointAtlasPages = 4;	// Point Light Atlas 최대 page 수
	};

public:
	FShadowOption Shadow;

	// --- 직렬화 ---
	void SaveToFile(const FString& Path) const;
	void LoadFromFile(const FString& Path);

	static FString GetDefaultPath() { return FPaths::ToUtf8(FPaths::ProjectSettingsFilePath()); }
};
