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

	bool bFocused = false;
	bool bHovered = false;
	bool bVisible = true;
};
