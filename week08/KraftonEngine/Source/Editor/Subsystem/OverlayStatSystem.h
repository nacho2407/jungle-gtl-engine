#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Pipeline/RenderConstants.h"

class UEditorEngine;

struct FPickingFrameStats
{
	double TotalMs = 0.0;
	double GizmoMs = 0.0;
	double WorldBVHMs = 0.0;
	double NarrowPhaseMs = 0.0;
	double MeshBVHMs = 0.0;
	uint32 WorldInternalNodesVisited = 0;
	uint32 WorldLeafNodesVisited = 0;
	uint32 PrimitiveAABBTests = 0;
	uint32 PrimitiveAABBHits = 0;
	uint32 PrimitiveNarrowPhaseCalls = 0;
	uint32 MeshInternalNodesVisited = 0;
	uint32 MeshLeafPacketsTested = 0;
	uint32 MeshTriangleLanesTested = 0;
};

struct FOverlayStatGroup
{
	TArray<FString> Lines;
};

struct FOverlayStatLayout
{
	float StartX = 16.0f;
	float StartY = 25.0f;
	float LineHeight = 20.0f;
	float GroupSpacing = 12.0f;
	float TextScale = 1.0f;
};

class FOverlayStatSystem
{
public:
	FOverlayStatSystem()
	{
#if FPS_OPTIMIZATION
		ShowFPS();
		ShowPickingTime();
#endif
	}

	void ShowFPS(bool bEnable = true) { bShowFPS = bEnable; }
	void ShowPickingTime(bool bEnable = true) { bShowPickingTime = bEnable; }
	void ShowMemory(bool bEnable = true) { bShowMemory = bEnable; }
	void RecordPickingAttempt(const FPickingFrameStats& Stats);
	void HideAll()
	{
		bShowFPS = false;
		bShowPickingTime = false;
		bShowMemory = false;
	}

	const FOverlayStatLayout& GetLayout() const { return Layout; }
	FOverlayStatLayout& GetLayout() { return Layout; }

	TArray<FOverlayStatGroup> BuildGroups(const UEditorEngine& Editor) const;
	TArray<FOverlayStatLine> BuildLines(const UEditorEngine& Editor) const;

private:
	bool bShowFPS = false;	// 260403 이번 경연 동안 fps는 항상 보이도록 설정.
	bool bShowPickingTime = false; // WM_LBUTTONDOWN , VK_LBUTTON 입력 시점이 아닌 오브젝트 충돌 판정에 걸린 시간을 측정합니다.
	bool bShowMemory = false;
	FPickingFrameStats LastPickingStats;
	double AccumulatedPickingTimeMs = 0.0;
	uint32 PickingAttemptCount = 0;

	FOverlayStatLayout Layout;
};
