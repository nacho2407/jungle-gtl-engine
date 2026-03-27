#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"

class UGizmoComponent;
class FLevelEditorViewportClient;

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

	// Viewport Client 관리 (UE: AllViewportClients / LevelViewportClients)
	const TArray<FEditorViewportClient*>& GetAllViewportClients() const { return AllViewportClients; }
	const TArray<FLevelEditorViewportClient*>& GetLevelViewportClients() const { return LevelViewportClients; }

	void RenderUI(float DeltaTime);

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;

	// UE 구조: AllViewportClients 는 모든 에디터 뷰포트, LevelViewportClients 는 레벨 편집 전용
	TArray<FEditorViewportClient*> AllViewportClients;
	TArray<FLevelEditorViewportClient*> LevelViewportClients;
};
