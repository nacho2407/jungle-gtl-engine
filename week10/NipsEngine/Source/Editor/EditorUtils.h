#pragma once
#include "Runtime/ViewportRect.h"
#include "Render/Common/ViewTypes.h"
/*
* Editor 모듈에서 필요한 Utility + Enum 정의
*/

// 뷰포트별 PIE 재생 상태를 나타냅니다.
// UEditorEngine::GetEditorState() / SetEditorState() 는 포커스된 뷰포트의 이 값을 읽고 씁니다.
enum class EViewportPlayState : uint8
{
    Editing,  // 편집 모드
    Playing,  // PIE 실행 중
    Paused,   // PIE 일시정지
};

struct FEditorViewportState
{
	EViewMode ViewMode = EViewMode::Lit;
	bool bHovered = false;

	// Stat Overlay (뷰포트별 독립 제어)
	bool bShowStatFPS       = false;
	bool bShowStatMemory    = false;
	bool bShowStatNameTable = false;

	// NameTable 오버레이 스크롤 오프셋 (휠로 조작)
	int32 NameTableScrollLine = 0;
};
