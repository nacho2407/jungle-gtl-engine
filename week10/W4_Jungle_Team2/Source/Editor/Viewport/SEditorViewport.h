#pragma once
#include "Slate/SWindow.h"

class SViewport;
class FEditorViewportClient;
class FSceneViewport;

class SEditorViewport : public SWindow
{
public:
	void SetViewportWidget(SViewport* InViewportWidget) { ViewportWidget = InViewportWidget;}
	SViewport* GetViewportWidget() const { return ViewportWidget; }

	FEditorViewportClient* GetViewportClient() const
	{
		return Client;
	}

protected:
	FSceneViewport* SceneViewport = nullptr;
	SViewport* ViewportWidget = nullptr;
	FEditorViewportClient* Client = nullptr;
};
