#include "WindowApplication.h"
#include "PlatformGlobals.h"
#include "Window.h"


TMap<HWND, CWindow*> CWindowApplication::WindowMap;

CWindowApplication& CWindowApplication::Get()
{
	static CWindowApplication Instance;
	return Instance;
}

bool CWindowApplication::Create(HINSTANCE InInstance, const WCHAR* ClassName)
{
	Instance = InInstance;
	GhInstance = InInstance;

	wcscpy_s(WindowClassName, ClassName);

	WindowClass = {};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.lpfnWndProc = StaticWndProc;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = WindowClassName;

	if (!RegisterClassEx(&WindowClass))
	{
		return false;
	}

	bClassRegistered = true;
	return true;
}

void CWindowApplication::Destroy()
{
	if (MainWindow)
	{
		delete MainWindow;
		MainWindow = nullptr;
	}

	if (bClassRegistered)
	{
		::UnregisterClassW(WindowClassName, Instance);
		bClassRegistered = false;
	}
}

CWindow* CWindowApplication::MakeWindow(const WCHAR* Title, int Width, int Height, int X, int Y)
{
	CWindow* Window = new CWindow();
	if (!Window->Create(Instance, WindowClassName, Title, Width, Height, X, Y))
	{
		delete Window;
		return nullptr;
	}
	return Window;
}

bool CWindowApplication::CreateMainWindow(const WCHAR* Title, int Width, int Height, int X, int Y)
{
	MainWindow = MakeWindow(Title, Width, Height, X, Y);
	return MainWindow != nullptr;
}

bool CWindowApplication::PumpMessages()
{
	MSG Msg = {};
	while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (Msg.message == WM_QUIT)
		{
			return false;
		}
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return true;
}

LRESULT CALLBACK CWindowApplication::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	auto It = WindowMap.find(hWnd);
	if (It != WindowMap.end())
	{
		return It->second->HandleMessage(Msg, wParam, lParam);
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

void CWindowApplication::RegisterWindow(HWND Hwnd, CWindow* Window)
{
	WindowMap[Hwnd] = Window;
}

void CWindowApplication::UnregisterWindow(HWND Hwnd)
{
	WindowMap.erase(Hwnd);
}

HWND CWindowApplication::GetHwnd() const
{
	return MainWindow ? MainWindow->GetHwnd() : nullptr;
}

int32 CWindowApplication::GetWindowWidth() const
{
	return MainWindow ? MainWindow->GetWidth() : 0;
}

int32 CWindowApplication::GetWindowHeight() const
{
	return MainWindow ? MainWindow->GetHeight() : 0;
}

void CWindowApplication::AddMessageFilter(FWndProcFilter Filter)
{
	if (MainWindow)
	{
		MainWindow->AddMessageFilter(std::move(Filter));
	}
}

void CWindowApplication::SetOnResizeCallback(FOnResizeCallback Callback)
{
	if (MainWindow)
	{
		MainWindow->SetOnResizeCallback(std::move(Callback));
	}
}

void CWindowApplication::ShowWindow()
{
	if (MainWindow)
	{
		MainWindow->Show();
	}
}
