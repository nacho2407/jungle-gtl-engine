#pragma once

#include "Viewport/ViewportTypes.h"
#include "SViewport.h"
#include "SplitterH.h"
#include "SplitterV.h"
#include "Widget.h"
#include <memory>
#include <functional>

class FSlateApplication
{
	FViewportId HoveredViewportId = INVALID_VIEWPORT_ID;
	FViewportId FocusedViewportId = INVALID_VIEWPORT_ID;
	FViewportId MouseCapturedViewportId = INVALID_VIEWPORT_ID;
	SSplitter*  DraggingSplitter = nullptr;

	FRect AreaRect;
	EViewportLayout CurrentLayout = EViewportLayout::Single;

	std::unique_ptr<SViewport> Viewports[MAX_VIEWPORTS];
	int32 ActiveViewportCount  = 0;

	std::unique_ptr<SSplitterH> SplitterPool_H[2];
	std::unique_ptr<SSplitterV> SplitterPool_V[2];

	SSplitter* ActiveSplitters[3]  = {};
	int32 ActiveSplitterCount = 0;

	SWidget* Root = nullptr;
	TArray<std::unique_ptr<SWidget>> OwnedWidgets;
	TArray<SWidget*> OverlayWidgets;

	EMouseCursor CurrentCursor = EMouseCursor::Default;

	void BuildTree_Single();
	void BuildTree_SplitH();
	void BuildTree_SplitV();
	void BuildTree_ThreeLeft();
	void BuildTree_ThreeRight();
	void BuildTree_ThreeTop();
	void BuildTree_ThreeBottom();
	void BuildTree_FourGrid();
	void ResetPools();
	void SyncViewportRects();

public:
	void Initialize(const FRect& Area, FViewport* VPs[], int32 Count);
	void SetLayout(EViewportLayout Layout);
	void SetViewportAreaRect(const FRect& Area);
	void PerformLayout();

	FViewportId GetHoveredViewportId() const { return HoveredViewportId; }
	FViewportId GetFocusedViewportId() const { return FocusedViewportId; }
	FViewportId GetMouseCapturedViewportId() const { return MouseCapturedViewportId; }
	EViewportLayout GetCurrentLayout() const { return CurrentLayout; }
	int32 GetActiveViewportCount() const { return ActiveViewportCount; }
	bool IsDraggingSplitter() const { return DraggingSplitter != nullptr; }
	bool IsPointerOverViewport(FViewportId Id) const { return HoveredViewportId == Id; }
	float GetSplitterRatio(int32 Index) const;
	void SetSplitterRatio(int32 Index, float Ratio);
	SWidget* CreateWidget(std::unique_ptr<SWidget> InWidget);
	void AddOverlayWidget(SWidget* W) { OverlayWidgets.push_back(W); }
	void Paint(SWidget& Painter);

	void ProcessMouseDown(int32 X, int32 Y);
	void ProcessMouseMove(int32 X, int32 Y);
	void ProcessMouseUp(int32 X, int32 Y);

	EMouseCursor GetCurrentCursor() const { return CurrentCursor; }
	std::function<void()> OnSplitterDragEnd;
};
