#pragma once
#include "CoreMinimal.h"
#include "WindowTypes.h"

class FWindowsWindow;

class ENGINE_API FWindowsApplication
{
public:
	static FWindowsApplication& Get();

	bool Create(HINSTANCE InInstance, const WCHAR* ClassName = L"JungleEngineWindow");
	void Destroy();

	FWindowsWindow* MakeWindow(const WCHAR* Title, int Width, int Height, int X = 100, int Y = 100);
	bool CreateMainWindow(const WCHAR* Title, int Width, int Height, int X = 100, int Y = 100);

	// Returns false when WM_QUIT received
	bool PumpMessages();

	HINSTANCE GetInstance() const { return Instance; }
	const WCHAR* GetClassName() const { return WindowClassName; }

	FWindowsWindow* GetMainWindow() const { return MainWindow; }
	HWND GetHwnd() const;
	int32 GetWindowWidth() const;
	int32 GetWindowHeight() const;

	void AddMessageFilter(FWndProcFilter Filter);
	void SetOnResizeCallback(FOnResizeCallback Callback);
	void ShowWindow();

private:
	FWindowsApplication() = default;
	~FWindowsApplication() = default;

	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	HINSTANCE Instance = nullptr;
	WNDCLASSEX WindowClass = {};
	WCHAR WindowClassName[128] = {};
	bool bClassRegistered = false;
	FWindowsWindow* MainWindow = nullptr;

	// HWND -> FWindowsWindow mapping
	static TMap<HWND, FWindowsWindow*> WindowMap;

	void RegisterWindow(HWND Hwnd, FWindowsWindow* Window);
	void UnregisterWindow(HWND Hwnd);

	friend class FWindowsWindow;
};
