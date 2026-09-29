#pragma once
#include "Slate/SWindow.h"

class SViewport;
class FEditorViewportClient;
class FSceneViewport;

class SEditorViewport : public SWindow
{
public:

	// Get Set
	FSceneViewport* GetSceneViewport() const { return SceneViewport; }
	void SetSceneViewport(FSceneViewport* InViewport) { SceneViewport = InViewport; }

	SViewport* GetViewport() const { return ViewportWidget; }
	void SetViewport(SViewport* InWidget) { ViewportWidget = InWidget; }

	FEditorViewportClient* GetEditorViewportClient() const { return Client; }
	void SetEditorViewportClient(FEditorViewportClient* InClient) { Client = InClient; }


protected:
	FSceneViewport* SceneViewport = nullptr;
	SViewport* ViewportWidget = nullptr;
	FEditorViewportClient* Client = nullptr;
};
