#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>

#include "Engine/Runtime/WindowsWindow.h"

using FOnSizingCallback = std::function<void()>;
using FOnResizedCallback = std::function<void(unsigned int, unsigned int)>;
using FOnCloseRequestedCallback = std::function<bool()>;

class FWindowsApplication
{
public:
	FWindowsApplication() = default;
	~FWindowsApplication() = default;

	bool Init(HINSTANCE InHInstance);
	void PumpMessages();
	void Destroy();

	FWindowsWindow& GetWindow() { return Window; }
	const FWindowsWindow& GetWindow() const { return Window; }

	bool IsExitRequested() const { return bIsExitRequested; }
	bool IsResizing() const { return bIsResizing; }

	void SetCustomTitleBarMetrics(int32 Height, const std::vector<FWindowHitTestRect>& InteractiveRects);
	void MinimizeWindow();
	void ToggleMaximizeWindow();
	void CloseWindow();
	bool IsWindowMaximized() const;
	const wchar_t* GetWindowTitle() const;

	void SetOnSizingCallback(FOnSizingCallback InCallback) { OnSizingCallback = std::move(InCallback); }
	void SetOnResizedCallback(FOnResizedCallback InCallback) { OnResizedCallback = std::move(InCallback); }
	void SetOnCloseRequestedCallback(FOnCloseRequestedCallback InCallback) { OnCloseRequestedCallback = std::move(InCallback); }

private:
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);
	bool HandleCustomChromeMessage(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam, LRESULT& OutResult);
	LRESULT HitTestCustomChrome(HWND hWnd, LPARAM lParam) const;
	void UpdateMaximizedBounds(HWND hWnd, MINMAXINFO* InOutMinMaxInfo) const;

private:
	HINSTANCE HInstance = nullptr;
	FWindowsWindow Window;

	bool bIsExitRequested = false;
	bool bIsResizing = false;

	FOnSizingCallback OnSizingCallback;
	FOnResizedCallback OnResizedCallback;
	FOnCloseRequestedCallback OnCloseRequestedCallback;
};
