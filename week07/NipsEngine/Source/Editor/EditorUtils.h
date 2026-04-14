#pragma once
#include "Runtime/ViewportRect.h"
#include "Render/Common/ViewTypes.h"
/*
* Editor 모듈에서 필요한 Utility + Enum 정의
*/

struct FEditorViewportState
{
	FViewportRect Rect;
	EViewMode ViewMode = EViewMode::Lit;
	bool bHovered = false;

	// Stat Overlay (뷰포트별 독립 제어)
	bool bShowStatFPS       = false;
	bool bShowStatMemory    = false;
	bool bShowStatNameTable = false;

	// NameTable 오버레이 스크롤 오프셋 (휠로 조작)
	int32 NameTableScrollLine = 0;
};

