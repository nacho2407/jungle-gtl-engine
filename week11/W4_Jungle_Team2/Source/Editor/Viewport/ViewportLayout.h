#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/StaticArray.h"
#include "EditorUtils.h"
#include "FSceneViewport.h"
#include "EditorViewportClient.h"

class UWorld;
class SViewport;
class SSplitterV;
class SSplitterH;
class FWindowsWindow;
class FViewportCamera;
class FSelectionManager;

/*
* Viewport Layout 을 관리하는 최상위 객체
* 스플리터 위젯 트리를 생성하고 FSlateApplication::RootWindow 에 연결합니다.
* SSplitterV → 2×SSplitterH → 4×SViewport
* SceneViewports[i] 를 각 SViewport 의 ISlateViewport 로 연결합니다.
*/

class FViewportLayout
{
public:
	static constexpr int32 MaxViewports = 4;

	// Lifecycle
	void Init(FWindowsWindow* InWindow, UWorld* World, FSelectionManager* SelectionManager);
	void Shutdown();
	void Tick(float DeltaTime);
	void OnWindowResized(uint32 Width, uint32 Height);

	FViewportCamera* GetIndexedViewportClientCamera(int32 Index) {
		return GetViewportClient(Index).GetCamera();
	}

	const FViewportCamera* GetIndexedViewportClientCamera(int32 Index) const {
		return GetViewportClient(Index).GetCamera();
	}

	// Splitter Get
	SSplitterV* GetRootSplitterV() const { return RootSplitterV; }
	SSplitterH* GetTopSplitterH()  const { return TopSplitterH; }
	SSplitterH* GetBotSplitterH()  const { return BotSplitterH; }

	// Viewport Get Set
	FEditorViewportClient& GetViewportClient(int32 Index) { return ViewportClients[Index]; }
	const FEditorViewportClient& GetViewportClient(int32 Index) const { return ViewportClients[Index]; }

	FSceneViewport& GetSceneViewport(int32 Index) { return SceneViewports[Index]; }
	const FSceneViewport& GetSceneViewport(int32 Index) const { return SceneViewports[Index]; }

	FEditorViewportState& GetViewportState(int32 Index) { return ViewportStates[Index]; }
	const FEditorViewportState& GetViewportState(int32 Index) const { return ViewportStates[Index]; }

	// Window 크기 기준으로 4개 뷰포트 영역을 계산합니다.
	void UpdateViewportRects(uint32 Width, uint32 Height);

	// Splitter Widget Tree 생성
	void BuildViewportLayout(int32 Width, int32 Height);

	// SViewport(FRect) → ISlateViewport(FViewportRect) 동기화
	// SplitRatio가 바뀌거나 창 크기가 바뀔 때 호출합니다.
	void SyncViewportRects();

	// 스플리터 위젯 소유권 (new → BuildViewportLayout, delete → DestroyViewportLayout)
	void DestroyViewportLayout();
private:
	// TODO: 1개 <-> 4개 왔다갔다 할 수 있게
	TStaticArray<FEditorViewportClient, MaxViewports> ViewportClients;
	TStaticArray<FSceneViewport, MaxViewports>        SceneViewports;
	TStaticArray<FEditorViewportState, MaxViewports>  ViewportStates;

	// Slate 위젯 트리 — UEditorEngine 이 소유합니다.
	SSplitterV* RootSplitterV = nullptr;
	SSplitterH* TopSplitterH = nullptr;
	SSplitterH* BotSplitterH = nullptr;
	SViewport* ViewportWidgets[MaxViewports] = {};

	// 캐싱 목적 Window 소유(소유권은 WindowsApplication)
	FWindowsWindow* Window = nullptr;
};

