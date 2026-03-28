#pragma once
#include "Slate/SlateUtils.h"
/*
* Editor 모듈에서 필요한 Utility + Enum 정의
*/

enum class EEditorCameraMode
{
	Perspective = 0,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};

struct FEditorViewportState
{
	FViewportRect Rect;
	EViewMode ViewMode = EViewMode::Lit;

	bool bFocused = false;
	bool bHovered = false;
	bool bVisible = true;
};
