#pragma once
#include "SWindow.h"

struct FViewportMouseEvent;
class FSceneViewport;
class ISlateViewport;

class SViewport : public SWindow
{

public:
	bool OnMouseMove(int X, int Y) override;
	bool OnMouseButtonDown(int Button, int X, int Y) override;
	bool OnMouseButtonUp(int Button, int X, int Y) override;
	bool OnMouseWheel(int Delta, int X, int Y) override;
	bool OnKeyDown(uint32 Key) override;
	bool OnKeyUp(uint32 Key) override;

private:
	FViewportMouseEvent MakeMouseEvent(int X, int Y) const;

private:
	ISlateViewport* ViewportInterface = nullptr;
};

