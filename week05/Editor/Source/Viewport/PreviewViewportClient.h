#pragma once

#include "CoreMinimal.h"
#include "Core/ViewportClient.h"

class FEditorUI;

class FPreviewViewportClient : public IViewportClient
{
public:
	FPreviewViewportClient(FEditorUI& InEditorUI, FString InPreviewContextName);
	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	void Tick(FEngine* Engine, float DeltaTime) override;
	void Render(FEngine* Engine, FRenderer* Renderer) override;
	ULevel* ResolveScene(FEngine* Engine) const override;

private:
	FEditorUI& EditorUI;
	FString PreviewContextName;
};
