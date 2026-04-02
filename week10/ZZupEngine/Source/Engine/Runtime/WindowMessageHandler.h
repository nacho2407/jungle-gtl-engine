#pragma once
#include <Core/CoreTypes.h>
/**
 * WndProc 가 호출할 인터페이스.
 * 현재 FWindowsApplication 에 콜백이 있지만, 멀티 viewport + Slate 구조로 가려면 
 * message handler 인터페이스가 더 좋다.
 */
class IWindowMessageHandler
{
public:
	virtual ~IWindowMessageHandler() = default;

	/*
	* Mouse Input
	*/
	virtual bool OnMouseMove(void* hwnd, int X, int Y) = 0;
	virtual bool OnMouseButtonDown(void* hwnd, int Button, int X, int Y) = 0;
	virtual bool OnMouseButtonUp(void* hwnd, int Button, int X, int Y) = 0;
	virtual bool OnMouseWheel(void* hwnd, int Delta, int X, int Y) = 0;

	/*
	* Key Input
	*/
	virtual bool OnKeyDown(void* hwnd, uint32 Key) = 0;
	virtual bool OnKeyUp(void* hwnd, uint32 Key) = 0;
	virtual bool OnChar(void* hwnd, uint32 Codepoint) = 0;

	/*
	* Focus, Window Resize
	*/
	virtual bool OnResize(void* hwnd, int Width, int Height) = 0;
	virtual bool OnSetFocus(void* hwnd) = 0;
	virtual bool OnKillFocus(void* hwnd) = 0;
};
