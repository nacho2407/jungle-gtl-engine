// 런타임 영역의 세부 동작을 구현합니다.
#include "Engine/Runtime/WindowsWindow.h"

void FWindowsWindow::Initialize(HWND InHWindow)
{
    HWindow = InHWindow;

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
