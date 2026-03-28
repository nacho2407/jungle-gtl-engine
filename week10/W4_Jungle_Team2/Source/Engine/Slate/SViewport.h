#pragma once
#include "SWindow.h"

struct FViewportMouseEvent;
class FSceneViewport;
class ISlateViewport;

class SViewport : public SWindow
{

public:
	bool OnMouseMove(int32 X, int32 Y) override;
	bool OnMouseButtonDown(int32 Button, int32 X, int32 Y) override;
	bool OnMouseButtonUp(int32 Button, int32 X, int32 Y) override;
	bool OnMouseWheel(int32 Delta, int32 X, int32 Y) override;
	bool OnKeyDown(uint32 Key) override;
	bool OnKeyUp(uint32 Key) override;

private:
	FViewportMouseEvent MakeMouseEvent(int32 X, int32 Y) const;

private:
	ISlateViewport* ViewportInterface = nullptr;
};

