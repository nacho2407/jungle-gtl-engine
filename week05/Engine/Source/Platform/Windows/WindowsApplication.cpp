#include "WindowsApplication.h"
#include "PlatformGlobals.h"
#include "WindowsWindow.h"


TMap<HWND, FWindowsWindow*> FWindowsApplication::WindowMap;

FWindowsApplication& FWindowsApplication::Get()
{
	static FWindowsApplication Instance;
	return Instance;
}

bool FWindowsApplication::Create(HINSTANCE InInstance, const WCHAR* ClassName)
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

void FWindowsApplication::Destroy()
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

FWindowsWindow* FWindowsApplication::MakeWindow(const WCHAR* Title, int Width, int Height, int X, int Y)
{
	FWindowsWindow* Window = new FWindowsWindow();
	if (!Window->Create(Instance, WindowClassName, Title, Width, Height, X, Y))
	{
		delete Window;
		return nullptr;
	}
	return Window;
}

bool FWindowsApplication::CreateMainWindow(const WCHAR* Title, int Width, int Height, int X, int Y)
{
	MainWindow = MakeWindow(Title, Width, Height, X, Y);
	return MainWindow != nullptr;
}

bool FWindowsApplication::PumpMessages()
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

LRESULT CALLBACK FWindowsApplication::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	auto It = WindowMap.find(hWnd);
	if (It != WindowMap.end())
	{
		return It->second->HandleMessage(Msg, wParam, lParam);
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

void FWindowsApplication::RegisterWindow(HWND Hwnd, FWindowsWindow* Window)
{
	WindowMap[Hwnd] = Window;
}

void FWindowsApplication::UnregisterWindow(HWND Hwnd)
{
	WindowMap.erase(Hwnd);
}

HWND FWindowsApplication::GetHwnd() const
{
	return MainWindow ? MainWindow->GetHwnd() : nullptr;
}

int32 FWindowsApplication::GetWindowWidth() const
{
	return MainWindow ? MainWindow->GetWidth() : 0;
}

int32 FWindowsApplication::GetWindowHeight() const
{
	return MainWindow ? MainWindow->GetHeight() : 0;
}

void FWindowsApplication::AddMessageFilter(FWndProcFilter Filter)
{
	if (MainWindow)
	{
		MainWindow->AddMessageFilter(std::move(Filter));
	}
}

void FWindowsApplication::SetOnResizeCallback(FOnResizeCallback Callback)
{
	if (MainWindow)
	{
		MainWindow->SetOnResizeCallback(std::move(Callback));
	}
}

void FWindowsApplication::ShowWindow()
{
	if (MainWindow)
	{
		MainWindow->Show();
	}
}
