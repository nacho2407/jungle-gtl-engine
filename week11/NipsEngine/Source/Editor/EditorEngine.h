#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Viewport/ViewportCamera.h"
#include "Editor/Viewport/ViewportLayout.h"

class UGizmoComponent;

enum class EEditorState : uint8
{
	Edit,     // 에셋 배치 및 편집 모드
	Play,     // PIE (Play In Editor) 실행 중
	Pause,    // PIE 일시정지
	Simulate  // 뷰포트 내 시뮬레이션 (선택 사항)
};

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
	FViewportCamera* GetCamera();
	const FViewportCamera* GetCamera() const;

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }
	const FSelectionManager& GetSelectionManager() const { return SelectionManager; }

	FViewportLayout& GetViewportLayout() { return ViewportLayout; }
	const FViewportLayout& GetViewportLayout() const { return ViewportLayout; }

	void RenderUI(float DeltaTime);

	EEditorState GetEditorState() const { return EditorState; }

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;	
	FViewportLayout ViewportLayout;
	EEditorState EditorState = EEditorState::Edit;
};
