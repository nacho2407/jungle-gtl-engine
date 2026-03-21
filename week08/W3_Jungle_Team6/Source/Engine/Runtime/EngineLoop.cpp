#include "Engine/Runtime/EngineLoop.h"

#include <windowsx.h>

#include "ImGui/imgui_impl_win32.h"

#include "Engine/Core/ConsoleHelper.h"
#include "Engine/Core/InputSystem.h"
#include "Component/PrimitiveComponent.h"

// ImGui Win32 메시지 핸들러
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK FEngineLoop::StaticWndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
	FEngineLoop* EngineLoop = reinterpret_cast<FEngineLoop*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (message == WM_NCCREATE)
	{
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		EngineLoop = reinterpret_cast<FEngineLoop*>(createStruct->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(EngineLoop));
	}

	if (EngineLoop)
	{
		return EngineLoop->WndProc(hWnd, message, wParam, lParam);
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT FEngineLoop::WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MOUSEWHEEL:
		InputSystem::AddScrollDelta(GET_WHEEL_DELTA_WPARAM(wParam));
		return 0;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			Editor.OnWindowResized(LOWORD(lParam), HIWORD(lParam));
		}
		return 0;
	case WM_ENTERSIZEMOVE:
		bIsResizing = true;
		return 0;
	case WM_EXITSIZEMOVE:
		bIsResizing = false;
		return 0;
	case WM_SIZING:
		TickFrame();
		return 0;
	default:
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	(void)nShowCmd;

	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, StaticWndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWindow = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1920, 1080,
		nullptr, nullptr, hInstance, this);

	if (!HWindow)
	{
		return false;
	}

	// 에디터 초기화 및 테스트용 샘플 액터 생성
	Editor.Create(HWindow);
	Editor.SpawnNewPrimitiveActor<UCubeComponent>(FVector(-3.f, 0, 0));
	Editor.BeginPlay();

	InitializeTiming();
	return true;
}

void FEngineLoop::InitializeTiming()
{
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&PrevTime);
	DeltaTime = 0.0f;
}

void FEngineLoop::TickFrame()
{
	QueryPerformanceCounter(&CurrTime);
	DeltaTime = static_cast<float>(CurrTime.QuadPart - PrevTime.QuadPart) / static_cast<float>(Frequency.QuadPart);
	PrevTime = CurrTime;

	float MainLoopFps = (DeltaTime > 1e-6f) ? (1.0f / DeltaTime) : 0.0f;
	Editor.SetMainLoopFPS(MainLoopFps);
	UObjectManager::Get().CollectGarbage();
	Editor.BeginFrame(DeltaTime);
	Editor.Update(DeltaTime);


	Editor.Render(DeltaTime);
	Editor.EndFrame();
}

int FEngineLoop::Run()
{
	while (!bIsExit)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}

		if (bIsExit)
		{
			break;
		}

		TickFrame();
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	Editor.Release();
	UObjectManager::Get().CollectGarbage();
}
