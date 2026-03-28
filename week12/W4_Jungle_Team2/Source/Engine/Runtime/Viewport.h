#pragma once
#include "Runtime/ViewportRect.h"
/*
* Viewport 가장 Base 클래스
* Rect / Local coordinate Helper
* Common Viewport Utility
*/
class FViewport
{
public:
	virtual ~FViewport() = default;

	virtual void SetRect(const FViewportRect& InRect) { Rect = InRect; }
	virtual FViewportRect GetRect() { return Rect; }
	virtual const FViewportRect& GetRect() const { return Rect; }

	bool ContainsPoint(int32 X, int32 Y) const { return Rect.Contains(X, Y); }
	void WindowToLocal(int32 X, int32 Y, int32& OutX, int32& OutY) const
	{ 
		return Rect.WindowToLocal(X, Y, OutX, OutY); 
	}

protected:
	FViewportRect Rect;
};


