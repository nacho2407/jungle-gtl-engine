#pragma once

#include "ObjViewerEngine.h"
#include "ObjViewerDetailsWindow.h"
#include "ObjViewerViewportSurface.h"

class FWindowsWindow;
class FRenderer;

class FObjViewerShell
{
public:
	void Initialize(FObjViewerEngine* InEngine);
	void SetupWindow(FWindowsWindow* InWindow);
	void AttachToRenderer(FRenderer* InRenderer);
	void DetachFromRenderer(FRenderer* InRenderer);
	void PrepareViewportSurface(FRenderer* Renderer);
	void Render();
	void RequestImportDialog(const FString& FilePath, const FString& ImportSource);

	const FObjViewerViewportSurface& GetViewportSurface() const { return ViewportSurface; }
	bool WantsViewportMouseInput() const { return bViewportHovered || bViewportCaptureActive; }
	bool WantsViewportKeyboardInput() const { return bViewportFocused || bViewportCaptureActive; }

private:
	void BuildDefaultLayout(unsigned int DockID);
	void DrawMenuBar();
	void DrawToolbarWindow();
	void DrawViewportWindow();
	void DrawImportDialog();
	void ResetPendingImportState();

private:
	FObjViewerEngine* Engine = nullptr;
	FWindowsWindow* MainWindow = nullptr;
	FRenderer* CurrentRenderer = nullptr;

	FObjViewerDetailsWindow DetailsWindow;
	FObjViewerViewportSurface ViewportSurface;

	bool bWindowSetup = false;
	bool bAttached = false;
	bool bLayoutInitialized = false;
	bool bViewportHovered = false;
	bool bViewportFocused = false;
	bool bViewportCaptureActive = false;
	bool bOpenImportDialogNextFrame = false;

	int32 DesiredViewportWidth = 0;
	int32 DesiredViewportHeight = 0;
	FString PendingImportPath;
	FObjImportSummary PendingImportOptions;
};
