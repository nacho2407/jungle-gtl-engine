#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Viewport/ViewportCamera.h"

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
	// Window 크기 기준으로 4개 뷰포트 영역을 계산
	void UpdateViewportRects(uint32 Width, uint32 Height);

	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;

	TStaticArray<FEditorViewportClient, MaxViewports> AllViewportClients;
	TStaticArray<FSceneViewport, MaxViewports>        SceneViewports;
	TStaticArray<FEditorViewportState, MaxViewports>  ViewportStates;
};
