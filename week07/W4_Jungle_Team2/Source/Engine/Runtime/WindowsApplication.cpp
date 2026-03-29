#include "Engine/Runtime/WindowsApplication.h"

#include <windowsx.h>

#include "Engine/Core/InputSystem.h"
#include "Engine/Slate/SlateApplication.h"

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

	FSlateApplication& SlateApplication = FSlateApplication::Get();

	switch (Msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MOUSEMOVE:
	{
		// ImGui 가 마우스를 캡처하지 않았거나, Slate 가 드래그 중이면 Slate 에 전달합니다.
		// (드래그 중이면 ImGui 영역 위로 마우스가 올라가도 드래그를 유지해야 합니다.)
		const int32 MX = GET_X_LPARAM(lParam);
		const int32 MY = GET_Y_LPARAM(lParam);
		SlateApplication.OnMouseMove((void*)hWnd, MX, MY);
		return 0;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		// ImGui 가 마우스를 사용 중이면 Slate 에 전달하지 않습니다.
		// (FSlateApplication 은 ImGui 를 모르므로 여기서 필터링합니다.)
		if (!InputSystem::Get().GetGuiInputState().bUsingMouse)
		{
			const int32 MX = GET_X_LPARAM(lParam);
			const int32 MY = GET_Y_LPARAM(lParam);
			SlateApplication.OnMouseButtonDown((void*)hWnd, 0, MX, MY);
		}
		return 0;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	{
		// 드래그 종료는 ImGui 캡처 여부와 무관하게 항상 전달합니다.
		const int32 MX = GET_X_LPARAM(lParam);
		const int32 MY = GET_Y_LPARAM(lParam);
		SlateApplication.OnMouseButtonUp((void*)hWnd, 0, MX, MY);
		return 0;
	}
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
	WNDCLASSW WndClass = { 0, StaticWndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&WndClass);

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
