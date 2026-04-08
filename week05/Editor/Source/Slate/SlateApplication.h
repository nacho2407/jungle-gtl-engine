#pragma once

#include "Viewport/ViewportTypes.h"
#include "Widget/SViewport.h"
#include "Widget/SplitterH.h"
#include "Widget/SplitterV.h"
#include "Widget/Widget.h"
#include "ViewportHost.h"
#include <memory>
#include <functional>

class FSlateApplication
{
	FViewportId HoveredViewportId = INVALID_VIEWPORT_ID;
	FViewportId FocusedViewportId = INVALID_VIEWPORT_ID;
	FViewportId MouseCapturedViewportId = INVALID_VIEWPORT_ID;
	SSplitter*  DraggingSplitter = nullptr;
	bool bViewportMaximized = false;
	EViewportLayout LayoutBeforeMaximize = EViewportLayout::FourGrid;
	FViewportId MaximizedViewportId = INVALID_VIEWPORT_ID;
	int32 SwappedViewportIndex = -1;
	float SavedSplitterRatios[3] = { 0.5f, 0.5f, 0.5f };

	FRect AreaRect;
	EViewportLayout CurrentLayout = EViewportLayout::Single;

	std::unique_ptr<SViewport> Viewports[MAX_VIEWPORTS];
	std::unique_ptr<SViewportHost> ViewportHosts[MAX_VIEWPORTS];
	int32 ActiveViewportCount  = 0;

	std::unique_ptr<SSplitterH> SplitterPool_H[2];
	std::unique_ptr<SSplitterV> SplitterPool_V[2];

	SSplitter* ActiveSplitters[3]  = {};
	int32 ActiveSplitterCount = 0;

	SWidget* Root = nullptr;
	TArray<std::unique_ptr<SWidget>> OwnedWidgets;
	TArray<SWidget*> OverlayWidgets;
	SWidget* GlobalChromeWidget = nullptr;
	SWidget* ViewportChromeWidgets[MAX_VIEWPORTS] = {};
	TArray<SWidget*> ChromePaintOrder;

	EMouseCursor CurrentCursor = EMouseCursor::Default;
	bool IsCursorInArea = false;

	void BuildTree_Single(); // 단일 뷰포트 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_SplitH(); // 가로 분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_SplitV(); // 세로 분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_ThreeLeft(); // 왼쪽 강조 3분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_ThreeRight(); // 오른쪽 강조 3분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_ThreeTop(); // 위쪽 강조 3분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_ThreeBottom(); // 아래쪽 강조 3분할 레이아웃용 위젯 트리를 구성한다.
	void BuildTree_FourGrid(); // 4분할 그리드 레이아웃용 위젯 트리를 구성한다.
	void ResetPools(); // 위젯 트리를 다시 만들기 전에 풀링된 상태를 비운다.
	void SyncViewportRects(); // 레이아웃 결과를 실제 뷰포트 사각형에 다시 반영한다.
	void RefreshChromeWidgetList(); // 그려야 할 크롬 위젯 목록을 다시 만든다.
	void LayoutChromeWidgets(); // 메인 트리 배치 후 오버레이와 크롬 위젯을 배치한다.
	FRect GetWorkspaceRect() const; // 글로벌 툴바 아래의 작업 영역을 반환한다.
	int32 GetGlobalToolbarHeight() const; // 글로벌 툴바가 차지하는 높이를 반환한다.
	int32 FindActiveViewportIndexById(FViewportId ViewportId) const; // 뷰포트 ID에 대응하는 활성 슬롯 인덱스를 찾는다.
	void ToggleViewportMaximize(FViewportId ViewportId); // 선택한 뷰포트를 최대화하거나 원복한다.
	SWidget* FindTopOverlayWidgetAt(FPoint Point) const; // 주어진 위치의 최상단 오버레이 위젯을 찾는다.
	SWidget* FindTopChromeWidgetAt(FPoint Point) const; // 주어진 위치의 최상단 크롬 위젯을 찾는다.
	void BringOverlayWidgetToFront(SWidget* Widget); // 오버레이 위젯 하나를 그리기 순서의 맨 앞으로 올린다.
	void BringChromeWidgetToFront(SWidget* Widget); // 크롬 위젯 하나를 그리기 순서의 맨 앞으로 올린다.

public:
	void Initialize(const FRect& Area, FViewport* VPs[], int32 Count); // 뷰포트 위젯과 초기 레이아웃 트리를 구성한다.
	void SetLayout(EViewportLayout Layout); // 활성 뷰포트 레이아웃 프리셋을 바꾼다.
	void FocusViewport(FViewportId ViewportId); // 지정한 활성 뷰포트로 포커스를 옮긴다.
	void SetViewportAreaRect(const FRect& Area); // 전체 뷰포트가 사용할 레이아웃 영역을 갱신한다.
	void PerformLayout(); // 현재 위젯 트리를 다시 만들고 배치한다.

	FViewportId GetHoveredViewportId() const { return HoveredViewportId; }
	FViewportId GetFocusedViewportId() const { return FocusedViewportId; }
	FViewportId GetMouseCapturedViewportId() const { return MouseCapturedViewportId; }
	EViewportLayout GetCurrentLayout() const { return CurrentLayout; }
	int32 GetActiveViewportCount() const { return ActiveViewportCount; }
	bool IsViewportActive(FViewportId Id) const; // 해당 뷰포트가 현재 활성 레이아웃에 포함되면 true를 반환한다.
	bool IsDraggingSplitter() const { return DraggingSplitter != nullptr; }
	bool IsPointerOverViewport(FViewportId Id) const { return HoveredViewportId == Id; }
	float GetSplitterRatio(int32 Index) const; // 현재 활성 레이아웃의 스플리터 비율을 반환한다.
	void SetSplitterRatio(int32 Index, float Ratio); // 현재 활성 레이아웃의 스플리터 비율을 갱신한다.
	SWidget* CreateWidget(std::unique_ptr<SWidget> InWidget); // 소유권을 보관하고 raw 포인터를 반환한다.
	void AddOverlayWidget(SWidget* W); // 레이아웃 트리 뒤에 그릴 오버레이 위젯을 등록한다.
	void BuildDrawList(FSlatePaintContext& Painter); // Slate 결과를 렌더러 비의존적인 드로우 리스트로 기록한다.
	void Paint(FSlatePaintContext& Painter); // 현재 위젯 트리를 전달된 페인트 컨텍스트에 그린다.

	void ProcessMouseDown(int32 X, int32 Y); // 위젯과 스플리터를 대상으로 마우스 다운 입력을 처리한다.
	void ProcessMouseDoubleClick(int32 X, int32 Y); // 위젯 대상 더블클릭 입력을 처리한다.
	void ProcessMouseMove(int32 X, int32 Y); // 현재 커서 위치 기준 hover, drag, splitter 갱신을 처리한다.
	void ProcessMouseUp(int32 X, int32 Y); // 현재 마우스 상호작용의 drag/capture 상태를 종료한다.

	EMouseCursor GetCurrentCursor() const { return CurrentCursor; }
	bool GetIsCoursorInArea() const { return IsCursorInArea; }
	std::function<void()> OnSplitterDragEnd;
};

