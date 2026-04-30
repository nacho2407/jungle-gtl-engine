#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/StaticArray.h"
#include "EditorUtils.h"
#include "FSceneViewport.h"
#include "EditorViewportClient.h"
#include "Engine/Slate/SViewport.h"

class UEditorEngine;
class UWorld;
class SViewport;
class SSplitterV;
class SSplitterH;
class SSplitterCross;
class FWindowsWindow;
class FViewportCamera;
class FSelectionManager;

/*
 * Viewport Layout 을 관리하는 최상위 객체
 * 스플리터 위젯 트리를 생성하고 FSlateApplication::RootWindow 에 연결합니다.
 * SSplitterV → 2×SSplitterH → 4×SViewport
 * SceneViewports[i] 를 각 SViewport 의 ISlateViewport 로 연결합니다.
 */

/**
 * 기존에 FViewportLayout 만 있었기 때문에 다형성을 위해 FEditorViewportLayout 과 분리
 */

class FViewportLayout
{
public:

private:
};

enum class EEditorViewportLayoutMode : uint8
{
	OnePane,
	TwoPanesHoriz,
	TwoPanesVert,
	ThreePanesLeft,
	ThreePanesRight,
	ThreePanesTop,
	ThreePanesBottom,
	FourPanes2x2,
	FourPanesLeft,
	FourPanesRight,
	FourPanesTop,
	FourPanesBottom,
	Max,
};

class FEditorViewportLayout : FViewportLayout
{
public:
	static constexpr int32 MaxViewports = 4;

	// Lifecycle
	void Init(FWindowsWindow* InWindow, UWorld* World, FSelectionManager* SelectionManager, UEditorEngine* EditorEngine);
	void Shutdown();
	void UpdateHoverStates();
	void Tick(float DeltaTime);
	void OnWindowResized(uint32 Width, uint32 Height);
	void SetHostRect(const FViewportRect& InHostRect);
	
	const FViewportClient* GetFocusedViewportClient() const { return GetViewportClient(LastFocusedViewportIndex); }

	FViewportCamera* GetIndexedViewportClientCamera(int32 Index) {
		return GetViewportClient(Index)->GetCamera();
	}

	const FViewportCamera* GetIndexedViewportClientCamera(int32 Index) const {
		return GetViewportClient(Index)->GetCamera();
	}

	// Splitter Get
	SSplitterV*    GetRootSplitterV() const { return RootSplitterV; }
	SSplitterH*    GetTopSplitterH()  const { return TopSplitterH; }
	SSplitterH*    GetBotSplitterH()  const { return BotSplitterH; }
	SSplitterCross* GetCrossWidget()  const { return CrossWidget; }

	// 1개 ↔ 4개 전환
	// bSingle=true  : Index 번 뷰포트만 전체 화면
	// bSingle=false : 4분할 레이아웃 복원
	void SetSingleViewportMode(bool bSingle, int32 Index = 0);

	bool  IsSingleViewportMode()        const { return bSingleViewport; }
	int32 GetSingleViewportIndex()      const { return SingleViewportIndex; }
	int32 GetLastFocusedViewportIndex() const { return LastFocusedViewportIndex; }
	EEditorViewportLayoutMode GetLayoutMode() const { return LayoutMode; }
	int32 GetActiveViewportCount() const;
	const FViewportRect& GetHostRect() const { return HostRect; }
	void SetLastFocusedViewportIndex(int32 Index);
	void SetLayoutMode(EEditorViewportLayoutMode InMode, int32 FocusIndex = -1);
	void SetLayoutModeAnimated(EEditorViewportLayoutMode InMode, int32 FocusIndex = -1);
	void ToggleViewportSplit();

	// Viewport Get Set
    FEditorViewportClient* GetViewportClient(int32 Index) { return ViewportWidgets[Index].GetSceneViewport().GetClient(); }
    const FEditorViewportClient* GetViewportClient(int32 Index) const { return ViewportWidgets[Index].GetSceneViewport().GetClient(); }

	FSceneViewport& GetSceneViewport(int32 Index) { return ViewportWidgets[Index].GetSceneViewport(); }
	const FSceneViewport& GetSceneViewport(int32 Index) const { return ViewportWidgets[Index].GetSceneViewport(); }

	FEditorViewportState& GetViewportState(int32 Index) { return ViewportWidgets[Index].GetSceneViewport().GetState(); }
    const FEditorViewportState& GetViewportState(int32 Index) const { return ViewportWidgets[Index].GetSceneViewport().GetState(); }

	// Window 크기 기준으로 4개 뷰포트 영역을 계산 및 초기화 합니다.
	void InitViewportRect(uint32 Width, uint32 Height);

	// Splitter Widget Tree 생성
	void BuildViewportLayout(int32 Width, int32 Height);

	// SViewport(FRect) → ISlateViewport(FViewportRect) 동기화
	// SplitRatio가 바뀌거나 창 크기가 바뀔 때 호출합니다.
	void SyncViewportRects();

	// 스플리터 위젯 소유권 (new → BuildViewportLayout, delete → DestroyViewportLayout)
	void DestroyViewportLayout();

	bool HasActiveOperationViewport() const
	{
		return ActiveOperationViewportIndex >= 0 && ActiveOperationViewportIndex < MaxViewports;
    }

private:
	void SetViewportRect(int32 Index, const FViewportRect& Rect);
	void ApplyPresetViewportRects(const FRect& FullRect);
	void ComputeLayoutRects(EEditorViewportLayoutMode InMode, int32 InSingleViewportIndex, const FRect& FullRect, FViewportRect (&OutRects)[MaxViewports]) const;
	void TickLayoutTransition(float DeltaTime);
	void EndLayoutTransition();
	static int32 GetLayoutSlotCount(EEditorViewportLayoutMode InMode);

private:
	// 1개 ↔ 4개 전환 상태
	bool  bSingleViewport          = false;
	int32 SingleViewportIndex      = 0;
	EEditorViewportLayoutMode LayoutMode = EEditorViewportLayoutMode::FourPanes2x2;
	EEditorViewportLayoutMode LastSplitLayoutMode = EEditorViewportLayoutMode::FourPanes2x2;
	bool bLayoutTransitionActive = false;
	float LayoutTransitionElapsed = 0.0f;
	float LayoutTransitionDuration = 0.16f;
	FViewportRect LayoutTransitionStartRects[MaxViewports] = {};
	FViewportRect LayoutTransitionTargetRects[MaxViewports] = {};

	// 마지막으로 카메라 조작(포커스)이 발생한 뷰포트 인덱스
	// stat 콘솔 명령의 적용 대상으로 사용됩니다.
	int32 LastFocusedViewportIndex = 0;

	// 현재 드래그 중인 뷰포트 인덱스 (없으면 -1)
	int32 ActiveOperationViewportIndex = -1;

	// UI에서 드래그를 시작하면 마우스 조작이 끝날 때까지 뷰포트에서의 조작을 막습니다.
	bool bBlockViewportOperationUntilRelease = false;

	// Slate 위젯 트리 — UEditorEngine 이 소유합니다.
	SSplitterV*    RootSplitterV = nullptr;
	SSplitterH*    TopSplitterH  = nullptr;
	SSplitterH*    BotSplitterH  = nullptr;
	SSplitterCross* CrossWidget  = nullptr;

	// Viewport 구조 재편 중 다형성 임시 제거
	SViewport ViewportWidgets[MaxViewports] = {};
    FEditorViewportClient ViewportClients[MaxViewports] = {};

	// 캐싱 목적 Window 소유(소유권은 WindowsApplication)
	FWindowsWindow* Window = nullptr;
	FViewportRect HostRect;
	UEditorEngine* Editor = nullptr;

private:
    int32 FindViewportIndexAt(int32 MouseX, int32 MouseY) const;
};

