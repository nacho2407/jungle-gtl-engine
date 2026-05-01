#pragma once

#include "Editor/UI/EditorWidget.h"

#include <functional>

class FEditorViewportOverlayWidget;
class FEditorSceneWidget;
class FEditorPlayStreamWidget;

class FEditorToolbarWidget : public FEditorWidget
{
public:
	void SetViewportOverlayWidget(FEditorViewportOverlayWidget* InViewportOverlayWidget);
	void SetSceneWidget(FEditorSceneWidget* InSceneWidget);
	void SetPlayStreamWidget(FEditorPlayStreamWidget* InPlayStreamWidget);
	void SetPIEViewportFullscreenCallback(std::function<void(bool)> InCallback);
	void SetPanelVisibilityRefs(
		bool* InShowConsole,
		bool* InShowControl,
		bool* InShowProperty,
		bool* InShowSceneManager,
		bool* InShowMaterialEditor,
		bool* InShowStatProfiler,
		bool* InShowEditorDebug,
		bool* InShowContentBrowser,
		bool* InShowUndoHistory,
		bool* InPIEViewportFullscreenEnabled);
	virtual void Render(float DeltaTime) override;
	bool OpenSceneFileDialog(FString& OutFilePath) const;
	bool SaveSceneFileDialog(FString& OutFilePath) const;

private:
	void RenderFilesMenu();
	void RenderEditMenu();
	void RenderViewMenu();
	void RenderSettingsMenu();
	void RenderHelpMenu();

	FEditorViewportOverlayWidget* ViewportOverlayWidget = nullptr;
	FEditorSceneWidget* SceneWidget = nullptr;
	FEditorPlayStreamWidget* PlayStreamWidget = nullptr;

	bool* bShowConsole = nullptr;
	bool* bShowControl = nullptr;
	bool* bShowProperty = nullptr;
	bool* bShowSceneManager = nullptr;
	bool* bShowMaterialEditor = nullptr;
	bool* bShowStatProfiler = nullptr;
	bool* bShowEditorDebug = nullptr;
	bool* bShowContentBrowser = nullptr;
	bool* bShowUndoHistory = nullptr;
	bool* bPIEViewportFullscreenEnabled = nullptr;
	std::function<void(bool)> PIEViewportFullscreenCallback;
};
