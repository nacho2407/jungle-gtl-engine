#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Viewport/ViewportCamera.h"
#include "Engine/Slate/SSplitterV.h"
#include "Engine/Slate/SSplitterH.h"
#include "Engine/Slate/SViewport.h"

class UGizmoComponent;

class UEditorEngine : public UEngine
{
public:
	DECLARE_CLASS(UEditorEngine, UEngine)

	UEditorEngine() = default;
	~UEditorEngine() override = default;

	// Lifecycle overrides
	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }

	// 퍼스펙티브 카메라(인덱스 0)를 반환합니다.
	FViewportCamera* GetCamera() { return AllViewportClients[0].GetCamera(); }
	const FViewportCamera* GetCamera() const { return AllViewportClients[0].GetCamera(); }

	

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	void RenderUI(float DeltaTime);

	// Viewport Get Set
	static constexpr int32 MaxViewports = 4;

	FEditorViewportClient& GetViewportClient(int32 Index) { return AllViewportClients[Index]; }
	const FEditorViewportClient& GetViewportClient(int32 Index) const { return AllViewportClients[Index]; }

	FSceneViewport& GetSceneViewport(int32 Index) { return SceneViewports[Index]; }
	const FSceneViewport& GetSceneViewport(int32 Index) const { return SceneViewports[Index]; }

	FEditorViewportState& GetViewportState(int32 Index) { return ViewportStates[Index]; }
	const FEditorViewportState& GetViewportState(int32 Index) const { return ViewportStates[Index]; }

private:
	// Window 크기 기준으로 4개 뷰포트 영역을 계산합니다.
	void UpdateViewportRects(uint32 Width, uint32 Height);

	/*
	 * 스플리터 위젯 트리를 생성하고 FSlateApplication::RootWindow 에 연결합니다.
	 *   SSplitterV → 2×SSplitterH → 4×SViewport
	 * SceneViewports[i] 를 각 SViewport 의 ISlateViewport 로 연결합니다.
	 */
	void BuildViewportLayout(int32 Width, int32 Height);

	// SViewport(FRect) → ISlateViewport(FViewportRect) 동기화
	// SplitRatio가 바뀌거나 창 크기가 바뀔 때 호출합니다.
	void SyncViewportRects();

	// 스플리터 위젯 소유권 (new → BuildViewportLayout, delete → DestroyViewportLayout)
	void DestroyViewportLayout();

	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;

	TStaticArray<FEditorViewportClient, MaxViewports> AllViewportClients;
	TStaticArray<FSceneViewport, MaxViewports>        SceneViewports;
	TStaticArray<FEditorViewportState, MaxViewports>  ViewportStates;

	// Slate 위젯 트리 — UEditorEngine 이 소유합니다.
	SSplitterV* RootSplitterV      = nullptr;
	SSplitterH* TopSplitterH       = nullptr;
	SSplitterH* BotSplitterH       = nullptr;
	SViewport*  ViewportWidgets[MaxViewports] = {};
};
