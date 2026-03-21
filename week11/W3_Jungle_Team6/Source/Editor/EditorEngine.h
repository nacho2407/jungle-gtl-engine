#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"

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
	UGizmoComponent* GetGizmo() const { return EditorGizmo; }

	void ResetCamera(UCameraComponent* InCamera);
	void ClearScene();
	void ResetViewport();
	void CloseScene();
	void NewScene();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	bool IsUpdateRateLimited() const { return FEditorSettings::Get().bLimitUpdateRate; }
	void SetUpdateRateLimited(bool bLimited) { FEditorSettings::Get().bLimitUpdateRate = bLimited; }
	int32 GetUpdateRate() const { return FEditorSettings::Get().UpdateRate; }
	void SetUpdateRate(int32 NewRate) { FEditorSettings::Get().UpdateRate = (NewRate < 1) ? 1 : NewRate; }

protected:
	void Render(float DeltaTime) override;

private:
	void BuildRenderCommands();

private:
	UGizmoComponent* EditorGizmo = nullptr;
	FEditorMainPanel MainPanel;
	FEditorViewportClient ViewportClient;
};
