#include "SlateApplication.h"
#include "SWindow.h"

void FSlateApplication::Initialize()
{
	RootWindow = new SWindow();
}

void FSlateApplication::Shutdown()
{
	// 위젯 참조를 먼저 비웁니다 (W-8: dangling pointer 방지)
	ClearWidgetRefs();

	// RootWindow 만 소유 — 위젯 트리 내부는 호출자(에디터)가 해제합니다.
	delete RootWindow;
	RootWindow = nullptr;
}

void FSlateApplication::Tick(float DeltaTime)
{
}

void FSlateApplication::Paint()
{
	if (!RootWindow) return;
	bool bHovered = (RootWindow == HoveredWidget);
	RootWindow->Paint();
}

bool FSlateApplication::OnMouseMove(void* hwnd, int32 X, int32 Y)
{
	// 드래그 중(CapturedWidget 존재): 캡처된 위젯에만 전달합니다.
	if (CapturedWidget)
	{
		CapturedWidget->OnMouseMove(X, Y);
		return true;
	}

	// 드래그 중이 아니면 HitTest 로 호버 위젯을 갱신합니다.
	SWidget* Hit = HitTest(X, Y);
	SetHoveredWidget(Hit);
	if (Hit)
	{
		Hit->OnMouseMove(X, Y);
		return true;
	}
	return false;
}

bool FSlateApplication::OnMouseButtonDown(void* hwnd, int32 Button, int32 X, int32 Y)
{
	SWidget* Hit = HitTest(X, Y);
	if (!Hit) return false;

	SetFocusedWidget(Hit);
	return Hit->OnMouseButtonDown(Button, X, Y);
	// SSplitter::OnMouseButtonDown 내부에서 SetCapturedWidget(this) 를 호출합니다.
}

bool FSlateApplication::OnMouseButtonUp(void* hwnd, int32 Button, int32 X, int32 Y)
{
	// 드래그 종료: 캡처된 위젯에 Up 이벤트를 전달하고 해제합니다.
	if (CapturedWidget)
	{
		CapturedWidget->OnMouseButtonUp(Button, X, Y);
		// SSplitter::OnMouseButtonUp 내부에서 SetCapturedWidget(nullptr) 를 호출합니다.
		return true;
	}

	SWidget* Hit = HitTest(X, Y);
	if (Hit)
	{
		return Hit->OnMouseButtonUp(Button, X, Y);
	}
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

void FSlateApplication::ClearWidgetRefs()
{
	FocusedWidget = nullptr;
	HoveredWidget = nullptr;
	CapturedWidget = nullptr;
}

SWidget* FSlateApplication::HitTest(int32 X, int32 Y)
{
	if (RootWindow)
		return RootWindow->HitTest(X, Y);
	return nullptr;
}
