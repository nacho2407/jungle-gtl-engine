#include "WindowsWindow.h"
#include "WindowsApplication.h"

FWindowsWindow::~FWindowsWindow()
{
	Destroy();
}

bool FWindowsWindow::Create(HINSTANCE Instance, const WCHAR* ClassName,
	const WCHAR* Title, int InWidth, int InHeight, int InX, int InY)
{
	Width = InWidth;
	Height = InHeight;
	PosX = InX;
	PosY = InY;

	// AdjustWindowRect to ensure client area matches requested size
	RECT rc = { 0, 0, Width, Height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	int WindowWidth = rc.right - rc.left;
	int WindowHeight = rc.bottom - rc.top;

	Hwnd = CreateWindowEx(
		0, ClassName, Title,
		WS_OVERLAPPEDWINDOW,
		PosX, PosY, WindowWidth, WindowHeight,
		nullptr, nullptr, Instance, nullptr
	);

	if (!Hwnd)
	{
		return false;
	}

	FWindowsApplication::Get().RegisterWindow(Hwnd, this);
	return true;
}

void FWindowsWindow::Destroy()
{
	if (Hwnd)
	{
		FWindowsApplication::Get().UnregisterWindow(Hwnd);
		::DestroyWindow(Hwnd);
		Hwnd = nullptr;
	}
}

void FWindowsWindow::Show()
{
	if (Hwnd)
	{
		::ShowWindow(Hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(Hwnd);
	}
}

void FWindowsWindow::Hide()
{
	if (Hwnd)
	{
		::ShowWindow(Hwnd, SW_HIDE);
	}
}

void FWindowsWindow::AddMessageFilter(FWndProcFilter Filter)
{
	MessageFilters.push_back(Filter);
}

void FWindowsWindow::SetOnResizeCallback(FOnResizeCallback Callback)
{
	OnResizeCallback = Callback;
}

LRESULT FWindowsWindow::HandleMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	// Run message filters (ImGui, picking, input, etc.)
	for (auto& Filter : MessageFilters)
	{
		if (Filter(Hwnd, Msg, wParam, lParam))
		{
			return TRUE;
		}
	}

	switch (Msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			Width = LOWORD(lParam);
			Height = HIWORD(lParam);
			if (OnResizeCallback)
			{
				OnResizeCallback(Width, Height);
			}
		}
		return 0;
	}

	return DefWindowProc(Hwnd, Msg, wParam, lParam);
}
