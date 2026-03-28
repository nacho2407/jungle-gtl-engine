#include "SlateApplication.h"
#include "SWindow.h"

void FSlateApplication::Initialize()
{

}

void FSlateApplication::Shutdown()
{

}

void FSlateApplication::SetRootWindow(class SWindow* InRootWindow)
{

}

SWindow* FSlateApplication::GetRootWindow() const
{
	return RootWindow;
}

void FSlateApplication::Tick(float DeltaTime)
{

}

void FSlateApplication::Paint()
{

}

bool FSlateApplication::OnMouseMove(void* hwnd, int X, int Y)
{
	return false;
}

bool FSlateApplication::OnMouseButtonDown(void* hwnd, int Button, int X, int Y)
{
	return false;
}

bool FSlateApplication::OnMouseButtonUp(void* hwnd, int Button, int X, int Y)
{
	return false;
}

bool FSlateApplication::OnMouseWheel(void* hwnd, int Delta, int X, int Y)
{
	return false;
}

bool FSlateApplication::OnKeyDown(void* hwnd, uint32 Key)
{
	return false;
}

bool FSlateApplication::OnKeyUp(void* hwnd, uint32 Key)
{
	return false;
}

bool FSlateApplication::OnChar(void* hwnd, uint32 Codepoint)
{
	return false;
}

bool FSlateApplication::OnResize(void* hwnd, int Width, int Height)
{
	return false;
}

bool FSlateApplication::OnSetFocus(void* hwnd)
{
	return false;
}

bool FSlateApplication::OnKillFocus(void* hwnd)
{
	return false;
}

SWidget* FSlateApplication::HitTest(int X, int Y)
{
	return nullptr;
}
