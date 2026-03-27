#pragma once

#include "CoreMinimal.h"
#include "Core/ViewportClient.h"

class CEditorUI;
class FWindowsWindow;

class CPreviewViewportClient : public IViewportClient
{
public:
	CPreviewViewportClient(CEditorUI& InEditorUI, FWindowsWindow* InMainWindow, FString InPreviewContextName);

	void Attach(FEngine* Engine, CRenderer* Renderer) override;
	void Detach(FEngine* Engine, CRenderer* Renderer) override;
	void Tick(FEngine* Engine, float DeltaTime) override;
	UScene* ResolveScene(FEngine* Engine) const override;

private:
	CEditorUI& EditorUI;
	FWindowsWindow* MainWindow = nullptr;
	FString PreviewContextName;
};
