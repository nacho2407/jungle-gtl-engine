#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"

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
	FViewportCamera* GetCamera() { return ViewportClient.GetCamera(); }
	const FViewportCamera* GetCamera() const { return ViewportClient.GetCamera(); }

	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	void RenderUI(float DeltaTime);

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FEditorViewportClient ViewportClient;
};
