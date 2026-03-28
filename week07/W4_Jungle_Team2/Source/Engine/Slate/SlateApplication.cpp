#include "SlateApplication.h"
#include "SWindow.h"

void FSlateApplication::Initialize()
{
	RootWindow = new SWindow();
}

void FSlateApplication::Shutdown()
{
	// RootWindow 만 소유 — 위젯 트리 내부는 호출자(에디터)가 해제합니다.
	delete RootWindow;
	RootWindow = nullptr;
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
	// RootWindow 크기만 갱신합니다.
	// 위젯 트리 재배치는 UEditorEngine::OnWindowResized 에서 처리합니다.
	if (RootWindow)
		RootWindow->SetRect({ 0.f, 0.f, static_cast<float>(Width), static_cast<float>(Height) });

	return true;
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
	if (RootWindow)
		return RootWindow->HitTest(X, Y);
	return nullptr;
}
