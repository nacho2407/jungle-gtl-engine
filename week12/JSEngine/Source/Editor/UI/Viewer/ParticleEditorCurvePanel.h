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
	EParticleCurveEditorTool ActiveTool = EParticleCurveEditorTool::Pan;

	void Clear()
	{
		Type = EParticleEditorSelectionType::None;
		EmitterIndex = -1;
		LODIndex = -1;
		ModuleIndex = -1;
	}
};
