#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget : public FEditorWidget
{
private:
	bool bExpanded = false;
	void RenderViewportSettings(float DeltaTime);
	void RenderDebugStats(float DeltaTime);
	void RenderSplitterBar();

public:
	void Render(float DeltaTime) override;

private:
	// 각 뷰포트 상단에 UE 스타일 View Mode 툴바를 그립니다.
	void RenderViewportToolbars();
};
