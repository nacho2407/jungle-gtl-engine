#pragma once

#include "Window.h"

constexpr int32 BARWIDTH = 8;

class SSplitter : public SWindow
{
public:
	float Ratio = 0.0f;
	SWindow* SideLT = nullptr;
	SWindow* SideRB = nullptr;

	virtual void ArrangeChildren() = 0;
	virtual void OnMouseMove(int32 X, int32 Y) = 0;
	virtual FRect GetSplitterBarRect() const = 0;
	void  OnMouseUp();
};