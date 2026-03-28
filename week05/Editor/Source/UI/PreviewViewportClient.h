#pragma once

#include "CoreMinimal.h"
#include "Core/ViewportClient.h"

class CEditorUI;
class CWindow;

class CPreviewViewportClient : public IViewportClient
{
public:
	CPreviewViewportClient(CEditorUI& InEditorUI, CWindow* InMainWindow, FString InPreviewContextName);

	void Attach(CCore* Core, CRenderer* Renderer) override;
	void Detach(CCore* Core, CRenderer* Renderer) override;
	void Tick(CCore* Core, float DeltaTime) override;
	UScene* ResolveScene(CCore* Core) const override;

private:
	CEditorUI& EditorUI;
	CWindow* MainWindow = nullptr;
	FString PreviewContextName;
};
