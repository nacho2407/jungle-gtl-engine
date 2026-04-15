#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Editor/Viewport/ViewportLayout.h"

class UGizmoComponent;
class FEditorRenderPipeline;

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
	virtual void WorldTick(float DeltaTime) override;

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }

	// 퍼스펙티브 카메라(인덱스 0)를 반환합니다.
	FViewportCamera* GetCamera();
	const FViewportCamera* GetCamera() const;

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();
	void ApplySpatialIndexMaintenanceSettings(UWorld* TargetWorld = nullptr);

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }
	const FSelectionManager& GetSelectionManager() const { return SelectionManager; }

	FViewportLayout& GetViewportLayout() { return ViewportLayout; }
	const FViewportLayout& GetViewportLayout() const { return ViewportLayout; }
	FEditorRenderPipeline* GetEditorRenderPipeline() const;

	FEditorMainPanel& GetMainPanel() { return MainPanel; }

	void RenderUI(float DeltaTime);

	// 포커스된 뷰포트가 참조하는 월드를 반환합니다.
	// 편집 중이면 에디터 월드, PIE 중이면 PIE 월드가 됩니다.
	UWorld* GetFocusedWorld() const
	{
		return ViewportLayout.GetViewportClient(ViewportLayout.GetLastFocusedViewportIndex()).GetFocusedWorld();
	}

	// 주의! Editor State가 따로 존재하는 것이 아닙니다. 에디터가 현재 포커스한 뷰포트의 상태를 Get/Set합니다.
	EViewportPlayState GetEditorState() const
	{
		return ViewportLayout.GetViewportClient(ViewportLayout.GetLastFocusedViewportIndex()).GetPlayState();
	}

	void SetEditorState(EViewportPlayState InState)
	{
		ViewportLayout.GetViewportClient(ViewportLayout.GetLastFocusedViewportIndex()).SetPlayState(InState);
	}

	// PIE 모드 컨트롤 함수
	void StartPlaySession();
	void PausePlaySession();
	void ResumePlaySession();
	void StopPlaySession();

	FWorldContext& RegisterWorld(UWorld* InWorld, EWorldType Type, const FName& Handler, const FString& Name);
	void UnregisterWorld(const FName& Handle);
	FName GetEditorWorldHandle() const;

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel  MainPanel;
	FViewportLayout   ViewportLayout;
	TMap<int32, FName> ViewportPIEHandles;  // 뷰포트 인덱스 → PIE 월드 컨텍스트 핸들
};
