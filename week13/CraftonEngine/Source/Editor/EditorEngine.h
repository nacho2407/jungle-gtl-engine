#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Viewport/OverlayStatSystem.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"

class UGizmoComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;

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
	UCameraComponent* GetCamera() const;

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	// 레이아웃에 위임
	const TArray<FEditorViewportClient*>& GetAllViewportClients() const { return ViewportLayout.GetAllViewportClients(); }
	const TArray<FLevelEditorViewportClient*>& GetLevelViewportClients() const { return ViewportLayout.GetLevelViewportClients(); }

	void SetActiveViewport(FLevelEditorViewportClient* InClient) { ViewportLayout.SetActiveViewport(InClient); }
	FLevelEditorViewportClient* GetActiveViewport() const { return ViewportLayout.GetActiveViewport(); }

	void ToggleViewportSplit() { ViewportLayout.ToggleViewportSplit(); }
	bool IsSplitViewport() const { return ViewportLayout.IsSplitViewport(); }

	void RenderViewportUI(float DeltaTime) { ViewportLayout.RenderViewportUI(DeltaTime); }

	bool IsMouseOverViewport() const { return ViewportLayout.IsMouseOverViewport(); }

	void RenderUI(float DeltaTime);

	FOverlayStatSystem& GetOverlayStatSystem() { return OverlayStatSystem; }
	const FOverlayStatSystem& GetOverlayStatSystem() const { return OverlayStatSystem; }

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FLevelViewportLayout ViewportLayout;
	FOverlayStatSystem OverlayStatSystem;
};
