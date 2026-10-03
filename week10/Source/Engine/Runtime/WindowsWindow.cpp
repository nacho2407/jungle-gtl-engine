#include "Engine/Runtime/WindowsWindow.h"

void FWindowsWindow::Initialize(HWND InHWindow, const wchar_t* InTitle)
{
	HWindow = InHWindow;
	Title = InTitle ? InTitle : L"";

	RECT Rect;
	GetClientRect(HWindow, &Rect);
	Width = static_cast<float>(Rect.right - Rect.left);
	Height = static_cast<float>(Rect.bottom - Rect.top);
}

void FWindowsWindow::OnResized(unsigned int InWidth, unsigned int InHeight)
{
	Width = static_cast<float>(InWidth);
	Height = static_cast<float>(InHeight);
}

POINT FWindowsWindow::ScreenToClientPoint(POINT ScreenPoint) const
{
	ScreenToClient(HWindow, &ScreenPoint);
	return ScreenPoint;
}

void FWindowsWindow::SetCustomTitleBarMetrics(int32 Height, const std::vector<FWindowHitTestRect>& InteractiveRects)
{
	CustomTitleBarState.TitleBarHeight = Height;
	CustomTitleBarState.InteractiveRects = InteractiveRects;
}

void FWindowsWindow::Minimize()
{
	if (HWindow)
	{
		ShowWindow(HWindow, SW_MINIMIZE);
	}
}

void FWindowsWindow::ToggleMaximize()
{
	if (!HWindow)
	{
		return;
	}

	ShowWindow(HWindow, IsWindowMaximized() ? SW_RESTORE : SW_MAXIMIZE);
}

void FWindowsWindow::Close()
{
	if (HWindow)
	{
		PostMessageW(HWindow, WM_CLOSE, 0, 0);
	}
}

bool FWindowsWindow::IsWindowMaximized() const
{
	return HWindow && IsZoomed(HWindow) != FALSE;
}
