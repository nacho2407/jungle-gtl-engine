#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "Xinput.lib")

#pragma comment (lib, "fmod_vc.lib")

#include <windows.h>

#include "Game.hpp"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	std::srand(std::time(0));

	WCHAR WindowClass[] = L"GTLWindowClass";

	WCHAR Title[] = L"Game Tech Lab";

	WNDCLASSW wndclass = {
		0,
		WndProc,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		WindowClass
	};

	RegisterClassW(&wndclass);

	RECT rect = { 0, 0, 1024, 1024 };
	AdjustWindowRect(&rect, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindowExW(0,
		WindowClass,
		Title,
		WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,   // 타이틀바 포함한 전체 크기
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	Game Game(hWnd);

	if (!Game.Initialize())
	{
		return 0;
	}

	Game.Run();

	Game.ShutDown();

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		{
			return true;
		}
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
