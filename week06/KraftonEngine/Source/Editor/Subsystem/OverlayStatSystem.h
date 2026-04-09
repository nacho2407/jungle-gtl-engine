#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Pipeline/RenderConstants.h"

class UEditorEngine;

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
	void RecordPickingAttempt(double ElapsedMs);
	void HideAll()
	{
		bShowFPS = false;
		bShowPickingTime = false;
		bShowMemory = false;
	}

	const FOverlayStatLayout& GetLayout() const { return Layout; }
	FOverlayStatLayout& GetLayout() { return Layout; }

	TArray<FOverlayStatGroup> BuildGroups(const UEditorEngine& Editor) const;
	void BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const;
	TArray<FOverlayStatLine> BuildLines(const UEditorEngine& Editor) const;

private:
	void AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Text) const;

	bool bShowFPS = false;	// 260403 이번 경연 동안 fps는 항상 보이도록 설정.
	bool bShowPickingTime = false; // WM_LBUTTONDOWN , VK_LBUTTON 입력 시점이 아닌 오브젝트 충돌 판정에 걸린 시간을 측정합니다.
	bool bShowMemory = false;
	double LastPickingTimeMs = 0.0;
	double AccumulatedPickingTimeMs = 0.0;
	uint32 PickingAttemptCount = 0;
	mutable FString CachedFPSLine;
	mutable FString CachedPickingLine;

	FOverlayStatLayout Layout;
};
