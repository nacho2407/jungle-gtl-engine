#pragma once
#include "SlateUtils.h"

class ISlateViewport
{
public:
	virtual ~ISlateViewport() = default;

	// 렌더링
	virtual void Draw() = 0;

	// 레이아웃 / 크기 갱신
	virtual void SetRect(const FViewportRect& InRect) = 0;
	virtual const FViewportRect& GetRect() const = 0;

	// 좌표 보조
	virtual bool ContainsPoint(int X, int Y) const = 0;
	virtual void WindowToLocal(int X, int Y, int& OutX, int& OutY) const = 0;

	// 마우스 입력
	virtual bool OnMouseMove(const FViewportMouseEvent& Ev) = 0;
	virtual bool OnMouseButtonDown(const FViewportMouseEvent& Ev) = 0;
	virtual bool OnMouseButtonUp(const FViewportMouseEvent& Ev) = 0;
	virtual bool OnMouseWheel(float Delta) = 0;

	// 키 입력
	virtual bool OnKeyDown(uint32 Key) = 0;
	virtual bool OnKeyUp(uint32 Key) = 0;
	virtual bool OnChar(uint32 Codepoint) = 0;
};
