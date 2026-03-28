#pragma once
#include "Slate/SlateUtils.h"
/*
* Editor 모듈에서 필요한 Utility + Enum 정의
*/
enum class EEditorViewMode
{
	Lit = 0,
	UnLit,
	Wireframe
};

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

	EEditorCameraMode CameraMode = EEditorCameraMode::Perspective;
	EEditorViewMode ViewMode = EEditorViewMode::Lit;

	bool bFocused = false;
	bool bHovered = false;
	bool bVisible = true;
};
