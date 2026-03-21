#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"

// ============================================================
// FEditorSettings — editor.ini를 통해 공유되는 에디터 설정
//
// 파트 D가 INI 읽기/쓰기 구현, 파트 B(Grid)·파트 A(카메라) 등이 참조
// ============================================================
struct FEditorSettings
{
	// 뷰 모드
	EViewModeIndex ViewMode = EViewModeIndex::Lit;

	// ShowFlags (비트 플래그)
	uint32 ShowFlags = SF_All;

	// 그리드 설정 (파트 B가 사용)
	float GridSpacing = 1.0f;
	int GridHalfLineCount = 50;

	// 카메라 감도 (파트 A가 사용)
	float CameraMoveSensitivity = 10.0f;
	float CameraRotateSensitivity = 0.3f;

	// ---- INI 파일 연동 ----

	// editor.ini에서 설정 로드
	void LoadFromFile(const FString& FilePath);

	// editor.ini에 설정 저장
	void SaveToFile(const FString& FilePath) const;

	// ---- ShowFlag 유틸리티 ----

	bool HasShowFlag(EEngineShowFlags Flag) const { return (ShowFlags & Flag) != 0; }
	void SetShowFlag(EEngineShowFlags Flag, bool bEnabled)
	{
		if (bEnabled) ShowFlags |= Flag;
		else          ShowFlags &= ~Flag;
	}

	static FEditorSettings& Get();
};
