#pragma once

#include "Editor/Viewer/ParticleEditorViewer.h"

enum class EParticleCurveEditorTool
{
	Pan,
	Zoom
};

struct FParticleCurveEditorState
{
	EParticleEditorSelectionType Type = EParticleEditorSelectionType::None;
	int32 EmitterIndex = -1;
	int32 LODIndex = -1;
	int32 ModuleIndex = -1;
	float CanvasPanTime = 0.0f;
	float CanvasPanValue = 0.0f;
	float CanvasZoomX = 1.0f;
	float CanvasZoomY = 1.0f;
	int32 SelectedCurveIndex = 0;
	char SelectedCurveChannel = '\0';
	int32 SelectedKeyIndex = -1;
	int32 ActiveKeyIndex = -1;
	bool bWantsDeleteKeyFocus = false;
	bool bPendingHorizontalFit = false;
	bool bPendingVerticalFit = false;
	EParticleCurveEditorTool ActiveTool = EParticleCurveEditorTool::Pan;

	void Clear()
	{
		Type = EParticleEditorSelectionType::None;
		EmitterIndex = -1;
		LODIndex = -1;
		ModuleIndex = -1;
		SelectedCurveIndex = 0;
		SelectedCurveChannel = '\0';
		SelectedKeyIndex = -1;
		ActiveKeyIndex = -1;
		bWantsDeleteKeyFocus = false;
		bPendingHorizontalFit = false;
		bPendingVerticalFit = false;
	}
};
