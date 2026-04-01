#pragma once

#include "Editor/UI/EditorWidget.h"

class FEditorViewportOverlayWidget;
class FEditorSceneWidget;

class FEditorToolbarWidget : public FEditorWidget
{
public:
	void SetViewportOverlayWidget(FEditorViewportOverlayWidget* InViewportOverlayWidget);
	void SetSceneWidget(FEditorSceneWidget* InSceneWidget);
	void SetPanelVisibilityRefs(
		bool* InShowConsole,
		bool* InShowControl,
		bool* InShowProperty,
		bool* InShowSceneManager,
		bool* InShowMaterialEditor,
		bool* InShowStatProfiler);
	virtual void Render(float DeltaTime) override;

private:
	bool OpenSceneFileDialog(FString& OutFilePath) const;
	bool SaveSceneFileDialog(FString& OutFilePath) const;
	void RenderFilesMenu();
	void RenderViewMenu();
	void RenderEditMenu();
	void RenderHelpMenu();

	FEditorViewportOverlayWidget* ViewportOverlayWidget = nullptr;
	FEditorSceneWidget* SceneWidget = nullptr;

	bool* bShowConsole = nullptr;
	bool* bShowControl = nullptr;
	bool* bShowProperty = nullptr;
	bool* bShowSceneManager = nullptr;
	bool* bShowMaterialEditor = nullptr;
	bool* bShowStatProfiler = nullptr;
};
