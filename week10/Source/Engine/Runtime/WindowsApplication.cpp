#include "Engine/Runtime/WindowsApplication.h"

#include <vector>
#include <windowsx.h>

#include "Engine/Input/CursorControl.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Slate/SlateApplication.h"
#include "Slate/SWidget.h"

// ImGui Win32 메시지 핸들러
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

namespace
{
int32 GetResizeBorderThicknessX()
{
	return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

int32 GetResizeBorderThicknessY()
{
	return GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}
}

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
	LRESULT CustomChromeResult = 0;
	if (HandleCustomChromeMessage(hWnd, Msg, wParam, lParam, CustomChromeResult))
	{
		return CustomChromeResult;
	}

	if (Msg == WM_CHAR)
	{
		const uint32_t Codepoint = static_cast<uint32_t>(wParam);
		if (Codepoint >= 0x20 && Codepoint != 0x7F)
		{
			InputSystem::Get().AddTextInput(Codepoint);
		}
	}

	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
	{
		return true;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();

	auto ShouldRouteToSlate = [&](int32 X, int32 Y)
	{
		const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
		return SlateApplication.GetCapturedWidget() != nullptr || GuiState.IsInViewportHost(X, Y);
	};

	// Slate가 ImGui 아래로 들어가기 때문에 imgui 관련 예외를 모두 처리합니다.
	switch (Msg)
	{
	case WM_CLOSE:
		if (OnCloseRequestedCallback && !OnCloseRequestedCallback())
		{
			return 0;
		}
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(FCursorControl::IsCursorHidden() ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
			return TRUE;
		}
		break;
	case WM_INPUT:
	{
		UINT DataSize = 0;
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &DataSize, sizeof(RAWINPUTHEADER)) != 0 || DataSize == 0)
		{
			break;
		}

		std::vector<BYTE> RawData(DataSize);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, RawData.data(), &DataSize, sizeof(RAWINPUTHEADER)) != DataSize)
		{
			break;
		}

		const RAWINPUT* RawInput = reinterpret_cast<const RAWINPUT*>(RawData.data());
		if (RawInput->header.dwType == RIM_TYPEMOUSE)
		{
			InputSystem::Get().AddRawMouseDelta(RawInput->data.mouse.lLastX, RawInput->data.mouse.lLastY);
		}
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		const int32 MX = GET_X_LPARAM(lParam);
		const int32 MY = GET_Y_LPARAM(lParam);
		if (ShouldRouteToSlate(MX, MY) && SlateApplication.OnMouseMove((void*)hWnd, MX, MY))
		{
			return 0;
		}
		break;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		const int32 MX = GET_X_LPARAM(lParam);
		const int32 MY = GET_Y_LPARAM(lParam);

		if (ShouldRouteToSlate(MX, MY))
		{
			SlateApplication.OnMouseButtonDown((void*)hWnd, 0, MX, MY);
		}
		break;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	{
		const int32 MX = GET_X_LPARAM(lParam);
		const int32 MY = GET_Y_LPARAM(lParam);

		if (ShouldRouteToSlate(MX, MY))
		{
			SlateApplication.OnMouseButtonUp((void*)hWnd, 0, MX, MY);
		}
		break;
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
	WCHAR Title[] = L"JS Engine";
	HICON WindowIcon = LoadIconW(HInstance, MAKEINTRESOURCEW(101));
	WNDCLASSW WndClass = { 0, StaticWndProc, 0, 0, HInstance, WindowIcon, LoadCursorW(nullptr, IDC_ARROW), 0, 0, WindowClass };

	RegisterClassW(&WndClass);

	const DWORD WindowStyle = WS_OVERLAPPEDWINDOW & ~WS_CAPTION;
	RECT WindowRect = { 0, 0, 1920, 1080 };
	AdjustWindowRectEx(&WindowRect, WindowStyle, FALSE, 0);

	HWND HWindow = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WindowStyle | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top,
		nullptr, nullptr, HInstance, this);

	if (!HWindow)
	{
		return false;
	}

	if (WindowIcon)
	{
		SendMessageW(HWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(WindowIcon));
		SendMessageW(HWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(WindowIcon));
	}

	SetWindowPos(
		HWindow,
		nullptr,
		0,
		0,
		0,
		0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	RAWINPUTDEVICE RawMouseDevice = {};
	RawMouseDevice.usUsagePage = 0x01;
	RawMouseDevice.usUsage = 0x02;
	RawMouseDevice.dwFlags = RIDEV_INPUTSINK;
	RawMouseDevice.hwndTarget = HWindow;
	RegisterRawInputDevices(&RawMouseDevice, 1, sizeof(RAWINPUTDEVICE));

	Window.Initialize(HWindow, Title);
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

void FWindowsApplication::SetCustomTitleBarMetrics(int32 Height, const std::vector<FWindowHitTestRect>& InteractiveRects)
{
	Window.SetCustomTitleBarMetrics(Height, InteractiveRects);
}

void FWindowsApplication::MinimizeWindow()
{
	Window.Minimize();
}

void FWindowsApplication::ToggleMaximizeWindow()
{
	Window.ToggleMaximize();
}

void FWindowsApplication::CloseWindow()
{
	Window.Close();
}

bool FWindowsApplication::IsWindowMaximized() const
{
	return Window.IsWindowMaximized();
}

const wchar_t* FWindowsApplication::GetWindowTitle() const
{
	return Window.GetTitle().c_str();
}

bool FWindowsApplication::HandleCustomChromeMessage(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam, LRESULT& OutResult)
{
	switch (Msg)
	{
	case WM_NCCALCSIZE:
		OutResult = 0;
		return true;
	case WM_NCACTIVATE:
		OutResult = 1;
		return true;
	case WM_GETMINMAXINFO:
		UpdateMaximizedBounds(hWnd, reinterpret_cast<MINMAXINFO*>(lParam));
		OutResult = 0;
		return true;
	case WM_NCHITTEST:
		OutResult = HitTestCustomChrome(hWnd, lParam);
		return true;
	default:
		return false;
	}
}

LRESULT FWindowsApplication::HitTestCustomChrome(HWND hWnd, LPARAM lParam) const
{
	POINT CursorPosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	POINT ClientPosition = CursorPosition;
	ScreenToClient(hWnd, &ClientPosition);

	RECT ClientRect = {};
	GetClientRect(hWnd, &ClientRect);

	const bool bCanResize =
		!Window.IsWindowMaximized() &&
		((GetWindowLongW(hWnd, GWL_STYLE) & WS_THICKFRAME) != 0);
	if (bCanResize)
	{
		const int32 ResizeBorderX = GetResizeBorderThicknessX();
		const int32 ResizeBorderY = GetResizeBorderThicknessY();

		const bool bOnLeft = ClientPosition.x >= 0 && ClientPosition.x < ResizeBorderX;
		const bool bOnRight = ClientPosition.x < ClientRect.right && ClientPosition.x >= ClientRect.right - ResizeBorderX;
		const bool bOnTop = ClientPosition.y >= 0 && ClientPosition.y < ResizeBorderY;
		const bool bOnBottom = ClientPosition.y < ClientRect.bottom && ClientPosition.y >= ClientRect.bottom - ResizeBorderY;

		if (bOnTop && bOnLeft) return HTTOPLEFT;
		if (bOnTop && bOnRight) return HTTOPRIGHT;
		if (bOnBottom && bOnLeft) return HTBOTTOMLEFT;
		if (bOnBottom && bOnRight) return HTBOTTOMRIGHT;
		if (bOnLeft) return HTLEFT;
		if (bOnRight) return HTRIGHT;
		if (bOnTop) return HTTOP;
		if (bOnBottom) return HTBOTTOM;
	}

	const FCustomTitleBarState& TitleBarState = Window.GetCustomTitleBarState();
	if (TitleBarState.TitleBarHeight > 0 &&
		ClientPosition.y >= 0 &&
		ClientPosition.y < TitleBarState.TitleBarHeight)
	{
		for (const FWindowHitTestRect& Rect : TitleBarState.InteractiveRects)
		{
			if (Rect.Contains(ClientPosition.x, ClientPosition.y))
			{
				return HTCLIENT;
			}
		}
		return HTCAPTION;
	}

	return HTCLIENT;
}

void FWindowsApplication::UpdateMaximizedBounds(HWND hWnd, MINMAXINFO* InOutMinMaxInfo) const
{
	if (!InOutMinMaxInfo)
	{
		return;
	}

	HMONITOR Monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (!Monitor)
	{
		return;
	}

	MONITORINFO MonitorInfo = {};
	MonitorInfo.cbSize = sizeof(MONITORINFO);
	if (!GetMonitorInfoW(Monitor, &MonitorInfo))
	{
		return;
	}

	const RECT& MonitorRect = MonitorInfo.rcMonitor;
	const RECT& WorkRect = MonitorInfo.rcWork;

	InOutMinMaxInfo->ptMaxPosition.x = WorkRect.left - MonitorRect.left;
	InOutMinMaxInfo->ptMaxPosition.y = WorkRect.top - MonitorRect.top;
	InOutMinMaxInfo->ptMaxSize.x = WorkRect.right - WorkRect.left;
	InOutMinMaxInfo->ptMaxSize.y = WorkRect.bottom - WorkRect.top;
	InOutMinMaxInfo->ptMaxTrackSize = InOutMinMaxInfo->ptMaxSize;
}
