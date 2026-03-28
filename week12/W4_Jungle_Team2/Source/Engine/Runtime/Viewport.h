#pragma once
#include "Slate/SlateUtils.h"
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
	const FViewportRect& GetRect() const { return Rect; }

	bool ContainsPoint(int X, int Y) const { return Rect.Contains(X, Y); }
	void WindowToLocal(int X, int Y, int& OutX, int& OutY) const 
	{ 
		return Rect.WindowToLocal(X, Y, OutX, OutY); 
	}

protected:
	FViewportRect Rect;
};


