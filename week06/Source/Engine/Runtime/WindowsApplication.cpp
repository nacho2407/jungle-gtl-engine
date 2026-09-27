#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Runtime/resource.h"

#include <windowsx.h>

#include "Engine/Input/InputSystem.h"

// ImGui Win32 메시지 핸들러
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK FWindowsApplication::StaticWndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	FWindowsApplication* App = reinterpret_cast<FWindowsApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (Msg == WM_NCCREATE)
	{
		CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		App = reinterpret_cast<FWindowsApplication*>(CreateStruct->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(App));
	}

	if (App)
	{
		return App->WndProc(hWnd, Msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT FWindowsApplication::WndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
	{
		return true;
	}

	switch (Msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MOUSEWHEEL:
		InputSystem::Get().AddScrollDelta(GET_WHEEL_DELTA_WPARAM(wParam));
		return 0;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			unsigned int Width = LOWORD(lParam);
			unsigned int Height = HIWORD(lParam);
			Window.OnResized(Width, Height);
			if (OnResizedCallback)
			{
				OnResizedCallback(Width, Height);
			}
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		bIsResizing = true;
		return 0;
	case WM_EXITSIZEMOVE:
		bIsResizing = false;
		return 0;
	case WM_SIZING:
		if (OnSizingCallback)
		{
			OnSizingCallback();
		}
		return 0;
	default:
		break;
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

bool FWindowsApplication::Init(HINSTANCE InHInstance)
{
	HInstance = InHInstance;

	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSEXW WndClass = {};
	WndClass.cbSize = sizeof(WNDCLASSEXW);
	WndClass.lpfnWndProc = StaticWndProc;
	WndClass.hInstance = HInstance;
	WndClass.hIcon = LoadIconW(HInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
	WndClass.hIconSm = LoadIconW(HInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
	WndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	WndClass.lpszClassName = WindowClass;

	RegisterClassExW(&WndClass);

	HWND HWindow = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1920, 1080,
		nullptr, nullptr, HInstance, this);

	if (!HWindow)
	{
		return false;
	}

	Window.Initialize(HWindow);
	return true;
}

void FWindowsApplication::PumpMessages()
{
	MSG Msg;
	while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);

		if (Msg.message == WM_QUIT)
		{
			bIsExitRequested = true;
			break;
		}
	}
}

void FWindowsApplication::Destroy()
{
	if (Window.GetHWND())
	{
		DestroyWindow(Window.GetHWND());
	}
}
