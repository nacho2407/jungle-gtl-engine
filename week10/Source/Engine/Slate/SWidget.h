

#pragma once
#include "Core/CoreTypes.h"
#include "SlateUtils.h"
class SWidget
{
public:
	virtual ~SWidget() {}

	virtual void Tick(float DeltaTime) {}
	virtual void Paint() {}

	virtual SWidget* HitTest(int32 X, int32 Y) = 0;

	virtual bool OnMouseMove(int32 X, int32 Y) { return false; }
	virtual bool OnMouseButtonDown(int32 Button, int32 X, int32 Y) { return false; }
	virtual bool OnMouseButtonUp(int32 Button, int32 X, int32 Y) { return false; }
	virtual bool OnMouseWheel(int32 Delta, int32 X, int32 Y) { return false; }

	virtual bool OnKeyDown(uint32 Key) { return false; }
	virtual bool OnKeyUp(uint32 Key) { return false; }
	virtual bool OnChar(uint32 Codepoint) { return false; }

	void SetRect(const FRect& InRect) { Rect = InRect; }

	FRect& GetRect() { return Rect; }
	const FRect& GetRect() const { return Rect; }

protected:
	FRect Rect;
};

