#pragma once

#include "CoreMinimal.h"
#include "Core/ViewportClient.h"

class CEditorUI;
class FWindowsWindow;

class CPreviewViewportClient : public IViewportClient
{
public:
	CPreviewViewportClient(CEditorUI& InEditorUI, FWindowsWindow* InMainWindow, FString InPreviewContextName);

	void Attach(FEngineRuntime* Runtime, CRenderer* Renderer) override;
	void Detach(FEngineRuntime* Runtime, CRenderer* Renderer) override;
	void Tick(FEngineRuntime* Runtime, float DeltaTime) override;
	UScene* ResolveScene(FEngineRuntime* Runtime) const override;

private:
	CEditorUI& EditorUI;
	FWindowsWindow* MainWindow = nullptr;
	FString PreviewContextName;
};
