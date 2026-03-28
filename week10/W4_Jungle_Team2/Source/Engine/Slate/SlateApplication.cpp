#include "SlateApplication.h"
#include "SWindow.h"

void FSlateApplication::Initialize()
{

}

void FSlateApplication::Shutdown()
{

}

void FSlateApplication::Tick(float DeltaTime)
{

}

void FSlateApplication::Paint()
{

}

bool FSlateApplication::OnMouseMove(void* hwnd, int32 X, int32 Y)
{
	return false;
}

bool FSlateApplication::OnMouseButtonDown(void* hwnd, int32 Button, int32 X, int32 Y)
{
	return false;
}

bool FSlateApplication::OnMouseButtonUp(void* hwnd, int32 Button, int32 X, int32 Y)
{
	return false;
}

bool FSlateApplication::OnMouseWheel(void* hwnd, int32 Delta, int32 X, int32 Y)
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

bool FSlateApplication::OnResize(void* hwnd, int32 Width, int32 Height)
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

SWidget* FSlateApplication::HitTest(int32 X, int32 Y)
{
	return nullptr;
}
