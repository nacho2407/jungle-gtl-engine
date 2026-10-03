#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/EditorWidget.h"

#include <functional>

enum class EEditorCommandId : uint8;
struct FEditorShortcut;
class FEditorViewportOverlayWidget;
class FEditorPlayStreamWidget;

class FEditorToolbarWidget : public FEditorWidget
{
public:
	void SetViewportOverlayWidget(FEditorViewportOverlayWidget* InViewportOverlayWidget);
	void SetPlayStreamWidget(FEditorPlayStreamWidget* InPlayStreamWidget);
	void SetPIEViewportFullscreenCallback(std::function<void(bool)> InCallback);
	void SetBuildGameCallback(std::function<void()> InCallback);
	void SetRuntimeUIPreviewOpenCallback(std::function<void()> InCallback);
	void SetActiveCommandHandlers(
		std::function<bool(const FEditorShortcut&)> InShortcutHandler,
		std::function<bool(EEditorCommandId)> InCommandHandler);
	void SetActiveMenuRenderer(std::function<bool()> InMenuRenderer);
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
		bool* InShowRuntimeUIPreview,
		bool* InShowProjectSettings,
		bool* InShowWorldSettings,
		bool* InPIEViewportFullscreenEnabled);
	virtual void Render(float DeltaTime) override;
	void ProcessShortcuts();
	void RenderMenuContents();
	bool OpenSceneFileDialog(FString& OutFilePath) const;
	bool SaveSceneFileDialog(FString& OutFilePath) const;

private:
	void RenderFilesMenu();
	void RenderEditMenu();
	void RenderBuildMenu();
	void RenderWindowMenu();
	void RenderSettingsMenu();
	void RenderHelpMenu();

	FEditorViewportOverlayWidget* ViewportOverlayWidget = nullptr;
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
	bool* bShowRuntimeUIPreview = nullptr;
	bool* bShowProjectSettings = nullptr;
	bool* bShowWorldSettings = nullptr;
	bool* bPIEViewportFullscreenEnabled = nullptr;
	std::function<void(bool)> PIEViewportFullscreenCallback;
	std::function<void()> BuildGameCallback;
	std::function<void()> RuntimeUIPreviewOpenCallback;
	std::function<bool(const FEditorShortcut&)> ActiveShortcutHandler;
	std::function<bool(EEditorCommandId)> ActiveCommandHandler;
	std::function<bool()> ActiveMenuRenderer;
};
